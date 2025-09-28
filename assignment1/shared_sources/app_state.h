

namespace nlohmann {

template <class T, int R, int C, int Options, int MR, int MC>
struct adl_serializer<Eigen::Matrix<T, R, C, Options, MR, MC>> {
    using Mat = Eigen::Matrix<T, R, C, Options, MR, MC>;
    static_assert(R != Eigen::Dynamic && C != Eigen::Dynamic,
                  "This serializer expects fixed-size Eigen matrices.");
    static_assert(std::is_arithmetic_v<T>,
                  "This serializer supports only arithmetic (real) scalar types.");

    static void to_json(json& j, const Mat& m) {
        j = json::array();
        for (int r = 0; r < R; ++r) {
            json row = json::array();
            for (int c = 0; c < C; ++c) row.push_back(m(r, c));
            j.push_back(std::move(row));
        }
    }

    static void from_json(const json& j, Mat& m) {
        assert(j.is_array() && static_cast<int>(j.size()) == R);    // Expect 2D array with exact row count

        for (int r = 0; r < R; ++r) {
            const auto& row = j[r];
            assert(row.is_array() && static_cast<int>(row.size()) == C);    // Expect 2D array with exact col count
            for (int c = 0; c < C; ++c)
                m(r, c) = row[c].get<T>();
        }
    }
};

} // namespace nlohmann

#include <iostream>
#include <fstream>
#include <fmt/core.h>

struct AppState {
    // Members
#define DECL_FIELD(T, name, init) T name { init };
    STATE_FIELDS(DECL_FIELD)
#undef DECL_FIELD

        // JSON I/O (nlohmann::json)
        friend void to_json(nlohmann::json& j, const AppState& s) {
#define TOJ(T, name, init) j[#name] = (const T&)s.name;
        STATE_FIELDS(TOJ)
#undef TOJ
    }

    friend void from_json(const nlohmann::json& j, AppState& s) {
#define FRJ(T, name, init) if (j.contains(#name)) s.name = j.at(#name).get<T>();
        STATE_FIELDS(FRJ)
#undef FRJ
    }

    static AppState parse(string json) {
        return nlohmann::json::parse(json);
    }
    static string dump(AppState state) {
        return nlohmann::json(state).dump(4);
    }

    static void saveAndMaybeCreateDirectories(const nlohmann::json& j, const filesystem::path& path)
    {
        filesystem::path p;
        if (path.is_relative())
        {
            p = filesystem::current_path();
            p /= path;
        }
        else p = path;

        if (!filesystem::exists(p.parent_path()))
            if (!filesystem::create_directories(p.parent_path()))
                fail(fmt::format("Could not create path {} for saving JSON state", p.parent_path().string()));

        ofstream f(p);
        f << j.dump(4);
        std::cerr << "Saved state to " << p.string() << endl;
    }

    void load(const filesystem::path& json) {
        if (!filesystem::exists(json))
        {
            std::cerr << "JSON state file " << json.string() << " not found!" << endl;
            return;
        }
        *this = parse(loadTextFile(json));
        std::cerr << "Loaded state from " << json.string() << endl;
    }

    void save(const filesystem::path& json) {
        saveAndMaybeCreateDirectories(nlohmann::json(*this), json);
    }

};
