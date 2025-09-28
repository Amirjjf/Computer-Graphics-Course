#pragma once

namespace nlohmann {

    template<>
    struct adl_serializer<Camera> {

        static void to_json(json& j, const Camera& c) {
            j = json::object();
            c.get_json(j);
        }

        static void from_json(const json& j, Camera& c) {
            c.set_json(j);
        }
    };

} // namespace nlohmann
