
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

        std::ofstream f(p);
        f << j.dump(4);
        std::cerr << "Saved state to " << p.string() << "\n";
    }

    void load(const filesystem::path& json) {
        if (!filesystem::exists(json))
        {
            std::cerr << "JSON state file " << json.string() << " not found!" << "\n";
            return;
        }
        *this = parse(loadTextFile(json));
        std::cerr << "Loaded state from " << json.string() << "\n";
    }

    void save(const filesystem::path& json) {
        saveAndMaybeCreateDirectories(nlohmann::json(*this), json);
    }

};
