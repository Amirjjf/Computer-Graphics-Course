#pragma once

namespace nlohmann {

    template<class T, int R, int C, int Options, int MR, int MC>
    struct adl_serializer<Eigen::Matrix<T, R, C, Options, MR, MC>> {
        using Mat = Eigen::Matrix<T, R, C, Options, MR, MC>;
        static_assert(R != Eigen::Dynamic && C != Eigen::Dynamic,
            "This serializer expects fixed-size Eigen matrices.");
        static_assert(std::is_arithmetic_v<T>,
            "This serializer supports only arithmetic (real) scalar types.");

        static void to_json(json& j, const Mat& m) {
            if constexpr (R == 1 || C == 1) {
                // Flatten vectors to 1D arrays: [x, y, z]
                j = json::array();
                for (int i = 0; i < m.size(); ++i) j.push_back(m(i));
            }
            else {
                // Matrices: [[r0...], [r1...], ...]
                j = json::array();
                for (int r = 0; r < R; ++r) {
                    json row = json::array();
                    for (int c = 0; c < C; ++c) row.push_back(m(r, c));
                    j.push_back(std::move(row));
                }
            }
        }

        static void from_json(const json& j, Mat& m) {
            if constexpr (R == 1 || C == 1) {
                // Accept both [x,y,z] and [[x],[y],[z]]
                if (j.is_array() && !j.empty() && !j.front().is_array()) {
                    assert(static_cast<int>(j.size()) == m.size());
                    for (int i = 0; i < m.size(); ++i) m(i) = j[i].get<T>();
                    return;
                }
            }
            assert(j.is_array() && static_cast<int>(j.size()) == R);
            for (int r = 0; r < R; ++r) {
                const auto& row = j[r];
                assert(row.is_array() && static_cast<int>(row.size()) == C);
                for (int c = 0; c < C; ++c) m(r, c) = row[c].get<T>();
            }
        }
    }; 

} // namespace nlohmann
