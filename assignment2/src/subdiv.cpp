
#include "app.h"

#include "subdiv.h"

#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <array>
#include <cmath>


// assumes vertices and indices are already filled in.
void MeshWithConnectivity::computeConnectivity()
{
	// assign default values. boundary edges (no neighbor on other side) are denoted by -1.
	neighborTris.assign(indices.size(), Vector3i{ -1, -1, -1 });
	neighborEdges.assign(indices.size(), Vector3i{ -1, -1, -1 });

	// bookkeeping: map edges (vert0, vert1) to (triangle, edge_number) pairs
	typedef std::map<std::pair<int, int>, std::pair<int, int>> edgemap_t;
	edgemap_t M;

	for (int i = 0; i < (int)indices.size(); ++i) {
		// vertex index is also an index for the corresponding edge starting at that vertex
		for (int j = 0; j < 3; ++j) {
			int v0 = indices[i][j];
			int v1 = indices[i][(j+1)%3];
			auto it = M.find(std::make_pair(v1, v0));
			if (it == M.end()) {
				// edge not found, add myself to mapping
				// (opposite direction than when finding because we look for neighbor edges)
				M[std::make_pair(v0, v1)] = std::make_pair(i, j);
			} else {
				if (it->second.first == -1)	{
					std::cerr << "Non-manifold edge detected\n";
				} else {
					// other site found, let's fill in the data
					int other_t = it->second.first;
					int other_e = it->second.second;

					neighborTris[i][j] = other_t;
					neighborEdges[i][j] = other_e;

					neighborTris[other_t][other_e] = i;
					neighborEdges[other_t][other_e] = j;

					it->second.first = -1;
				}
			}
		}
	}
	
}

using std::min, std::max;

void MeshWithConnectivity::traverseOneRing(int i, int j, Vector3f& position, Vector3f& normal, Vector3f& color, vector<int>* debug_indices) const{

	// YOUR CODE HERE (R5):
	// Compute the new position, normal and color for the even (old) vertices
	// Implement the loop through neighbors using the connectivity information

	// debug_indices should be only provided when calling from app.cpp to construct the debug visualization. When actually computing the subdivision in
	// MeshWithConnectivity::LoopSubdivision() you should give a nullptr instead.


	if(i<0 || j<0){
		return;
	}
	int v0 = indices[i][j];

	// Walk around the one-ring of v0 using connectivity.
	int start_tri = i;
	int start_edge = j; // edge starting at v0
	int ct = start_tri;
	int ce = start_edge;

	Vector3f sumPos = Vector3f::Zero();
	Vector3f sumCol = Vector3f::Zero();
	Vector3f sumNrm = Vector3f::Zero();
	int n = 0;

    // If we hit a boundary at any point, leave the vertex unchanged per assignment note
    bool boundary = false;

	do {
		// Neighbor vertex at head of current edge (v0 -> v_head)
		int v_head = indices[ct][(ce + 1) % 3];
		if (debug_indices) debug_indices->push_back(v_head);
		sumPos += positions[v_head];
		sumCol += colors[v_head];
		sumNrm += normals[v_head];
		++n;

		// Move to adjacent triangle across the edge that ends at v0 in current triangle
		int e_in = (ce + 2) % 3; // edge v_prev -> v0
        int nt = neighborTris[ct][e_in];
        int ne = neighborEdges[ct][e_in];
        if (nt == -1 || ne == -1) {
            boundary = true;
            if (debug_indices) debug_indices->push_back(-1); // mark boundary for debug drawing
            break;
        }

		// In neighbor triangle, the shared edge will start at v0; continue from there
		ct = nt;
		ce = ne; // this edge starts at v0 in the neighbor triangle

	} while (!(ct == start_tri && ce == start_edge) && n <= (int)positions.size());

	if (boundary || n == 0) {
		// Do nothing; keep original position, normal, color
		return;
	}

	// Loop even-vertex rule
	float nf = float(n);
	const float PI = 3.14159265358979323846f;
	float theta = 2.0f * PI / nf;
	float beta = (5.0f/8.0f - std::pow(3.0f/8.0f + 0.25f * std::cos(theta), 2.0f)) / nf;
	float w_center = 1.0f - nf * beta;

	Vector3f newPos = w_center * positions[v0] + beta * sumPos;
	Vector3f newCol = w_center * colors[v0]    + beta * sumCol;
	Vector3f newNrm = w_center * normals[v0]   + beta * sumNrm;
	if (newNrm.norm() > 1e-8f) newNrm.normalize();

	position = newPos;
	color = newCol;
	normal = newNrm;
}


void MeshWithConnectivity::LoopSubdivision(DrawMode mode, bool crude_boundaries) {
	// generate new (odd) vertices

	// visited edge -> vertex position information
	// Note that this is different from the one in computeConnectivity()
	typedef std::map<std::pair<int, int>, int> edgemap_t;
	edgemap_t new_vertices;

	// The new data must be doublebuffered or otherwise some of the calculations below would
	// not read the original positions but the newly changed ones, which is slightly wrong.
	vector<Vector3f> new_positions(positions.size());
	vector<Vector3f> new_normals(normals.size());
	vector<Vector3f> new_colors(colors.size());
	vector<int>      new_ages(ages.size());

	// Precompute boundary flags and boundary neighbors per vertex
    std::vector<bool> isBoundaryVertex(positions.size(), false);
    std::vector<std::pair<int,int>> boundaryNeighbors(positions.size(), std::make_pair(-1, -1));
    auto addBoundaryNeighbor = [&](int v, int nb){
        auto &p = boundaryNeighbors[v];
        if (p.first == -1) p.first = nb;
        else if (p.second == -1 && p.first != nb) p.second = nb;
    };
    for (size_t ti = 0; ti < indices.size(); ++ti) {
        const auto &tri = indices[ti];
        for (int e = 0; e < 3; ++e) {
            if (neighborTris[ti][e] == -1) {
                int a = tri[e];
                int b = tri[(e+1)%3];
                isBoundaryVertex[a] = true;
                isBoundaryVertex[b] = true;
                addBoundaryNeighbor(a, b);
                addBoundaryNeighbor(b, a);
            }
        }
    }

	// Helper: robustly find the two boundary neighbors of a boundary vertex v using connectivity
	auto findBoundaryNeighbors = [&](int v, int triIdx, int edgeIdx) -> std::pair<int,int> {
		int nb0 = -1, nb1 = -1;
		int ct = triIdx, ce = edgeIdx;
		const int start_tri = triIdx, start_edge = edgeIdx;
		int guard = 0;
		do {
			// Check the two edges incident to v in this triangle
			// Edge starting at v
			if (neighborTris[ct][ce] == -1) {
				int head = indices[ct][(ce + 1) % 3];
				if (nb0 == -1) nb0 = head; else if (nb1 == -1 && head != nb0) nb1 = head;
			}
			// Edge ending at v
			int e_in = (ce + 2) % 3;
			if (neighborTris[ct][e_in] == -1) {
				int prev = indices[ct][(ce + 2) % 3]; // vertex at the other end of the edge ending at v
				if (nb0 == -1) nb0 = prev; else if (nb1 == -1 && prev != nb0) nb1 = prev;
			}
			if (nb0 != -1 && nb1 != -1) break;
			// Walk to next triangle around v via the edge ending at v
			int nt = neighborTris[ct][(ce + 2) % 3];
			int ne = neighborEdges[ct][(ce + 2) % 3];
			if (nt == -1 || ne == -1) break; // reached mesh boundary
			ct = nt; ce = ne;
			if (++guard > (int)indices.size()) break; // safety
		} while (!(ct == start_tri && ce == start_edge));
		return { nb0, nb1 };
	};

    for (size_t i = 0; i < indices.size(); ++i)
        for (int j = 0; j < 3; ++j) {
            int v0 = indices[i][j];
            int v1 = indices[i][(j + 1) % 3];

			// Map the edge endpoint indices to new vertex index.
			// We use min and max because the edge direction does not matter when we finally
			// rebuild the new faces (R3); this is how we always get unique indices for the map.
			auto edge = std::make_pair(min(v0, v1), max(v0, v1));

			// With naive iteration, we would find each edge twice, because each is part of two triangles
			// (if the mesh does not have any holes/empty borders). Thus, we keep track of the already
			// visited edges in the new_vertices map. That requires the small R3 task below in the 'if' block.
            if (new_vertices.find(edge) == new_vertices.end()) {
                // YOUR CODE HERE (R4): compute the position for odd (= new) vertex.
                // You will need to use the neighbor information to find the correct vertices and then combine the four corner vertices with the correct weights.
                // Be sure to see section 3.2 in the handout for an in depth explanation of the neighbor index tables; the scheme is somewhat involved.
                Vector3f pos, col, norm;

                // The default implementation just puts the new vertex at the edge midpoint.
                pos = 0.5f * (positions[v0] + positions[v1]);
                col = 0.5f * (colors[v0] + colors[v1]);
                norm = 0.5f * (normals[v0] + normals[v1]);

                // Only do this if "R3 & R4" or the full subdivision mode are selected in the UI
                // (this allows you to see the different stages separately)
                if (mode >= DrawMode::Subdivision_R3_R4) {
                    // Loop edge rule: new (odd) vertex at edge midpoint refined using opposite vertices
                    // Identify the two opposite vertices: one in current triangle i, one in neighboring triangle across edge (if any)
                    int opp_curr = indices[i][(j + 2) % 3];
                    int tri_nb = neighborTris[i][j];
                    int edge_nb = neighborEdges[i][j];
                    int opp_nb = -1;
                    if (tri_nb != -1 && edge_nb != -1) {
                        opp_nb = indices[tri_nb][(edge_nb + 2) % 3];
                    }
                    bool edge_is_boundary = (neighborTris[i][j] == -1);
                    if (!crude_boundaries && edge_is_boundary) {
                        // Proper boundary rule for odd vertex: stay at midpoint
                        pos = 0.5f * (positions[v0] + positions[v1]);
                        col = 0.5f * (colors[v0] + colors[v1]);
                        norm = 0.5f * (normals[v0] + normals[v1]);
                    } else {
                        // Interior edge rule (or crude handling): 3/8 endpoints + 1/8 opposites
                        const float w_end = 3.0f / 8.0f;
                        const float w_opp = 1.0f / 8.0f;

                        Vector3f p2 = (opp_curr >= 0) ? positions[opp_curr] : Vector3f::Zero();
                        Vector3f p3 = (opp_nb   >= 0) ? positions[opp_nb]   : Vector3f::Zero();
                        Vector3f c2 = (opp_curr >= 0) ? colors[opp_curr]   : Vector3f::Zero();
                        Vector3f c3 = (opp_nb   >= 0) ? colors[opp_nb]     : Vector3f::Zero();
                        Vector3f n2 = (opp_curr >= 0) ? normals[opp_curr]  : Vector3f::Zero();
                        Vector3f n3 = (opp_nb   >= 0) ? normals[opp_nb]    : Vector3f::Zero();

                        pos = w_end * (positions[v0] + positions[v1]) + w_opp * (p2 + p3);
                        col = w_end * (colors[v0] + colors[v1]) + w_opp * (c2 + c3);
                        norm = w_end * (normals[v0] + normals[v1]) + w_opp * (n2 + n3);
                    }
                    if (norm.norm() > 1e-8f) norm.normalize();
                }

				new_positions.push_back(pos);
				new_colors.push_back(col);
				new_normals.push_back(norm);
				new_ages.push_back(0); // odd vertices are newly created this level

				// YOUR CODE HERE (R3):
				// Map the edge to the correct vertex index.
				// This is just one line! Use new_vertices and the index of the position that was just pushed back to the vector.
				new_vertices[edge] = static_cast<int>(new_positions.size()) - 1;
			}
		}
    // compute positions for even (old) vertices
	vector<bool> vertexComputed(new_positions.size(), false);

	for (int i = 0; i < (int)indices.size(); ++i) {
		for (int j = 0; j < 3; ++j) {
			int v0 = indices[i][j];

			// don't redo if this one is already done
			if (vertexComputed[v0] )
				continue;

			vertexComputed[v0] = true;


			// YOUR CODE HERE (R5): reposition the old vertices

			// This default implementation just passes the data through unchanged.
			// You need to replace these three lines with the loop over the 1-ring
			// around vertex v0, and compute the new position as a weighted average
			// of the other vertices as described in the handout.

			// If you're having a difficult time, you can try debugging your implementation
			// with the debug highlight mode. If you press alt, traverseOneRing will be called
			// for only the vertex under your mouse cursor, which should help with debugging.

			Vector3f pos(Vector3f::Zero());
			Vector3f norm(Vector3f::Zero());
			Vector3f col(Vector3f::Zero());

                pos = positions[v0];
                col = colors[v0];
                norm = normals[v0];

                // Only do this if the full subdivision mode is selected in the UI
                if (mode == DrawMode::Subdivision) {
					if (isBoundaryVertex[v0]) {
                        if (!crude_boundaries) {
                            // Proper boundary rule for even (old) vertices on boundary:
                            // v' = 3/4 v + 1/8 (v_prev + v_next)
							// Prefer connectivity-based neighbors; fall back to precomputed if needed
							auto nbs = findBoundaryNeighbors(v0, i, j);
							int b0 = (nbs.first  != -1) ? nbs.first  : boundaryNeighbors[v0].first;
							int b1 = (nbs.second != -1) ? nbs.second : boundaryNeighbors[v0].second;
                            if (b0 != -1 && b1 != -1) {
                                const float w_c = 3.0f / 4.0f;
                                const float w_b = 1.0f / 8.0f;
                                pos = w_c * positions[v0] + w_b * (positions[b0] + positions[b1]);
                                col = w_c * colors[v0]    + w_b * (colors[b0]    + colors[b1]);
                                norm = w_c * normals[v0]   + w_b * (normals[b0]   + normals[b1]);
                                if (norm.norm() > 1e-8f) norm.normalize();
                            }
                            // if missing neighbors (degenerate), keep original
                        } // else crude: keep original (no change)
                    } else {
                        // Interior vertex: use standard Loop even-vertex rule via one-ring traversal
                        traverseOneRing(i, j, pos, norm, col, nullptr);
                    }
                }

			new_positions[v0] = pos;
			new_colors[v0] = col;
			new_normals[v0] = norm;
			// Age: even vertices survive to next level, increment age if present
			if (v0 >= 0 && v0 < (int)ages.size()) new_ages[v0] = ages[v0] + 1; else new_ages[v0] = 1;
		}
	}



	// and then, finally, regenerate topology
	// every triangle turns into four new ones
	std::vector<Vector3i> new_indices;
	new_indices.reserve(indices.size()*4);
	for (size_t i = 0; i < indices.size(); ++i) {
		Vector3i even = indices[i]; // start vertices of e_0, e_1, e_2

		// YOUR CODE HERE (R3):
		// fill in X and Y (it's the same for both)
		int X, Y;
		// edge a between even[0] and even[1]
		X = even[0]; Y = even[1];
		auto edge_a = std::make_pair(min(X, Y), max(X, Y));
		// edge b between even[1] and even[2]
		X = even[1]; Y = even[2];
		auto edge_b = std::make_pair(min(X, Y), max(X, Y));
		// edge c between even[2] and even[0]
		X = even[2]; Y = even[0];
		auto edge_c = std::make_pair(min(X, Y), max(X, Y));

		// The edges edge_a, edge_b and edge_c now define the vertex indices via new_vertices.
		// (The mapping is done in the loop above.)
		// The indices define the smaller triangle inside the indices defined by "even", in order.
		// Read the vertex indices out of new_vertices to build the small triangle "odd"

		Vector3i odd = Vector3i{ new_vertices[edge_a], new_vertices[edge_b], new_vertices[edge_c] };

		// Then, construct the four smaller triangles from the surrounding big triangle  "even"
		// and the inner one, "odd". Push them to "new_indices".

		// Maintain winding consistent with original: split into 3 outer + 1 inner triangle
		new_indices.push_back(Vector3i(even[0], odd[0], odd[2]));
		new_indices.push_back(Vector3i(even[1], odd[1], odd[0]));
		new_indices.push_back(Vector3i(even[2], odd[2], odd[1]));
		new_indices.push_back(Vector3i(odd[0], odd[1], odd[2]));
	}

	// ADD THESE LINES when R3 is finished. Replace the originals with the repositioned data.
	indices = std::move(new_indices);
	positions = std::move(new_positions);
	colors = std::move(new_colors);
	normals = std::move(new_normals);
	ages = std::move(new_ages);

	const bool showAgePalette =
		(mode == DrawMode::Subdivision ||
		 mode == DrawMode::Subdivision_R3 ||
		 mode == DrawMode::Subdivision_R3_R4);

	// Colorize by age to visualize smoothness development with a perceptually pleasing gradient.
	if (showAgePalette && !ages.empty()) {
		int maxAge = 0;
		for (int a : ages) maxAge = std::max(maxAge, a);
		float denom = static_cast<float>(std::max(1, maxAge));
		static const std::array<float, 5> stops{ 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
		static const std::array<Vector3f, 5> palette{
			Vector3f(0.145f, 0.196f, 0.498f),  // deep blue
			Vector3f(0.125f, 0.615f, 0.604f),  // teal
			Vector3f(0.773f, 0.905f, 0.461f),  // soft lime
			Vector3f(0.992f, 0.731f, 0.258f),  // sunflower
			Vector3f(0.902f, 0.318f, 0.420f)   // sunset magenta
		};
		for (size_t vi = 0; vi < colors.size(); ++vi) {
			float t = std::clamp(ages[vi] / denom, 0.0f, 1.0f);
			// slight easing to emphasize younger ages while keeping continuity
			t = std::pow(t, 0.85f);
			size_t idx = 0;
			while (idx + 1 < stops.size() && t > stops[idx + 1]) ++idx;
			size_t next = std::min(idx + 1, stops.size() - 1);
			float span = stops[next] - stops[idx];
			float local = (span > 1e-6f) ? (t - stops[idx]) / span : 0.0f;
			Vector3f col = (1.0f - local) * palette[idx] + local * palette[next];
			colors[vi] = col;
		}
	}
	// Recompute normals from updated geometry for better shading, especially near boundaries
	computeVertexNormals();
	if (!showAgePalette) {
		colorizeByCurvature();
	}
}

void MeshWithConnectivity::colorizeByCurvature(float gamma, float percentile)
{
	if (positions.empty())
		return;

	percentile = std::clamp(percentile, 0.0f, 0.999f);
	if (colors.size() != positions.size())
		colors.assign(positions.size(), Vector3f(0.82f, 0.82f, 0.82f));

	std::vector<Vector3f> neighborSum(positions.size(), Vector3f::Zero());
	std::vector<int> valence(positions.size(), 0);
	auto accumulate = [&](int a, int b)
	{
		neighborSum[a] += positions[b];
		++valence[a];
	};

	for (const auto& tri : indices)
	{
		int i0 = tri[0];
		int i1 = tri[1];
		int i2 = tri[2];
		accumulate(i0, i1); accumulate(i0, i2);
		accumulate(i1, i0); accumulate(i1, i2);
		accumulate(i2, i0); accumulate(i2, i1);
	}

	std::vector<float> signedCurv(positions.size(), 0.0f);
	std::vector<float> magnitudes;
	magnitudes.reserve(positions.size());
	float maxAbs = 0.0f;

	for (size_t i = 0; i < positions.size(); ++i)
	{
		if (valence[i] == 0)
			continue;
		Vector3f n = normals[i];
		float n2 = n.squaredNorm();
		if (!(n2 > 1e-12f))
			continue;
		n /= std::sqrt(n2);
		Vector3f mean = neighborSum[i] / static_cast<float>(valence[i]);
		Vector3f lap = mean - positions[i];
		float h = -lap.dot(n);
		signedCurv[i] = h;
		float absh = std::abs(h);
		magnitudes.push_back(absh);
		if (absh > maxAbs)
			maxAbs = absh;
	}

	Vector3f base(0.82f, 0.82f, 0.82f);
	if (magnitudes.empty() || maxAbs < 1e-8f)
	{
		for (size_t i = 0; i < colors.size(); ++i)
			colors[i] = base;
		return;
	}

	size_t nthIndex = static_cast<size_t>(percentile * float(magnitudes.size() - 1));
	if (nthIndex >= magnitudes.size())
		nthIndex = magnitudes.size() - 1;
	std::nth_element(magnitudes.begin(), magnitudes.begin() + nthIndex, magnitudes.end());
	float scale = magnitudes[nthIndex];
	if (scale < 1e-8f)
		scale = maxAbs;

	auto mix = [](const Vector3f& a, const Vector3f& b, float t)
	{
		return (1.0f - t) * a + t * b;
	};

	Vector3f warmLo(0.98f, 0.68f, 0.20f);
	Vector3f warmHi(0.85f, 0.16f, 0.05f);
	Vector3f coolLo(0.30f, 0.80f, 1.00f);
	Vector3f coolHi(0.05f, 0.25f, 0.70f);

	for (size_t i = 0; i < positions.size(); ++i)
	{
		if (valence[i] == 0)
		{
			colors[i] = base;
			continue;
		}
		float h = signedCurv[i];
		float absH = std::abs(h);
		float normalized = scale > 1e-8f ? std::min(absH / scale, 1.0f) : 0.0f;
		float weight = gamma > 0.0f ? std::pow(normalized, gamma) : normalized;
		if (weight < 1e-4f)
		{
			colors[i] = base;
			continue;
		}
		if (h >= 0.0f)
		{
			Vector3f accent = mix(warmLo, warmHi, weight);
			colors[i] = mix(base, accent, weight);
		}
		else
		{
			Vector3f accent = mix(coolLo, coolHi, weight);
			colors[i] = mix(base, accent, weight);
		}
	}
}

MeshWithConnectivity* MeshWithConnectivity::loadOBJ(const string& filename, bool crude_boundary)
{
	// Open input file stream for reading.
	std::ifstream input(filename, std::ios::in);

	// vertex and index arrays read from OBJ
	vector<Vector3f> positions;
	vector<Vector3i> faces;

	// Read the file line by line.
	string line;
	while (getline(input, line)) {
		for (auto& c : line)
			if (c == '/')
				c = ' ';

		Vector3i  f; // vertex indices
		Vector3f  v;
		string    s;

		// Create a stream from the string to pick out one value at a time.
		std::istringstream        iss(line);

		// Read the first token from the line into string 's'.
		// It identifies the type of object (vertex or normal or ...)
		iss >> s;

		if (s == "v") { // vertex position
			iss >> v[0] >> v[1] >> v[2];
			positions.push_back(v);
		}
		else if (s == "f") { // face
			iss >> f[0] >> f[1] >> f[2];
			f -= Vector3i{ 1, 1, 1 }; // go to zero-based indices from OBJ's one-based
			faces.push_back(f);
		}
	}

	// deduplicate vertices (lexicographical comparator CompareVector3f defined in app.h)
	// first, insert all positions into a search structure
	typedef std::map<Vector3f, unsigned, CompareVector3f> postoindex_t;
	postoindex_t vmap;
	for (auto& v : positions)
		if (vmap.find(v) == vmap.end()) // if this position wasn't there..
			vmap[v] = vmap.size();      // put it in and mark down its index (==current size of map)

	// Construct mesh:
	// Insert the unique vertices into their right places in the vertex array,
	// and compute bounding box while we're at it
	MeshWithConnectivity* pMesh = new MeshWithConnectivity();
	pMesh->positions.resize(vmap.size());
	pMesh->colors.resize(vmap.size());
	pMesh->normals.resize(vmap.size());
	pMesh->ages.resize(vmap.size());
	Eigen::Array3f bbmin{ FLT_MAX, FLT_MAX, FLT_MAX };
	Eigen::Array3f bbmax = -bbmin;
	for (auto& unique_vert : vmap)
	{
		pMesh->positions[unique_vert.second] = unique_vert.first;
		pMesh->colors[unique_vert.second]    = Vector3f{ 0.75f, 0.75f, 0.75f };
	pMesh->normals[unique_vert.second]   = Vector3f::Zero();
	pMesh->ages[unique_vert.second]      = 0; // original mesh vertices start at age 0
		bbmin = bbmin.min(unique_vert.first.array());
		bbmax = bbmax.max(unique_vert.first.array());
	}

	// set up indices: loop over all faces, look up deduplicated vertex indices by using position from original array
	pMesh->indices.resize(faces.size());
	for (size_t i = 0; i < faces.size(); ++i)
	{
		int i0 = vmap[positions[faces[i][0]]];  // the unique index of the 3D position vector referred to by 0th index of faces[i], etc.
		int i1 = vmap[positions[faces[i][1]]];
		int i2 = vmap[positions[faces[i][2]]];
		pMesh->indices[i] = Vector3i{ i0, i1, i2 };
	}

	// center mesh to origin and normalize scale
	// first construct scaling and translation matrix
	float scale = 10.0f / Vector3f(bbmax - bbmin).norm();
	Vector3f ctr = 0.5f * Vector3f(bbmin + bbmax);
	Matrix4f ST{
		{ scale,  0.0f,  0.0f, -ctr(0) * scale },
		{  0.0f, scale,  0.0f, -ctr(1) * scale },
		{  0.0f,  0.0f, scale, -ctr(2) * scale },
		{  0.0f,  0.0f,  0.0f,            1.0f }
	};
	// then apply it to all vertices
	for (auto& v : pMesh->positions)
	{
		Vector4f v4;
		v4 << v, 1.0f;
		v4 = ST * v4;
		v = v4.block(0, 0, 3, 1);
	}
	// put in the vertex normals..
	pMesh->computeVertexNormals();
	pMesh->computeConnectivity();

	return pMesh;
}

std::tuple<int,int> MeshWithConnectivity::pickTriangle(const Vector3f& o, const Vector3f& d) const
{
	float mint = FLT_MAX;
	int rettri = -1;
	int retind = -1;
	for (size_t i = 0; i < indices.size(); ++i)
	{
		const Vector3f& p0 = positions[indices[i](0)];
		const Vector3f& p1 = positions[indices[i](1)];
		const Vector3f& p2 = positions[indices[i](2)];
		Matrix3f T;
		T << p0 - p1, p0 - p2, d;
		Vector3f b = p0 - o;
		Vector3f x = T.inverse() * b;

		float b1 = x(0);
		float b2 = x(1);
		float t = x(2);

		if (b1 >= 0.0f &&
			b2 >= 0.0f &&
			b1 + b2 <= 1.0f &&
			t > 0 &&
			t < 1 &&
			t < mint)
		{
			mint = t;
			rettri = i;

			Vector3f hit = o+t*d;
			retind = (p0-hit).norm()<(p1-hit).norm()?0:1;
			retind = std::min((p0-hit).norm(), (p1-hit).norm())<(p2-hit).norm()?retind:2;
		}
	}
	return std::make_tuple(rettri, retind);
}
