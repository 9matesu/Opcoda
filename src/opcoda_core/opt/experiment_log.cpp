#include "experiment_log.h"

#include <cstdio>
#include <fstream>
#include <limits>

namespace opcoda::opt {

std::string experimentCsvHeader() noexcept {
    return "instance_id,n,instance_seed,instance_kind,method,parameters,"
           "initial_cost,final_cost,iterations,evaluations,attempts,time_ms,"
           "seed,repetition";
}

std::string experimentCsvRow(const ExperimentRecord& r) noexcept {
    const auto fmtCost = [](double c) {
        if (c == std::numeric_limits<double>::infinity()) {
            return std::string {"inf"};
        }
        if (c < 0.0) {
            return std::string {"-inf"};
        }
        // tres casas decimais: custa um pouco mas o leitor nao perde precisao.
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.3f", c);
        return std::string(buf);
    };

    char buf[1024];
    const int n = std::snprintf(
        buf, sizeof(buf),
        "%s,%d,%llu,%s,%s,%s,%.3f,%.3f,%ld,%ld,%d,%.3f,%llu,%d",
        r.instanceId.c_str(), r.nodes,
        static_cast<unsigned long long>(r.instanceSeed),
        r.instanceKind.c_str(), r.method.c_str(), r.parameters.c_str(),
        r.initialCost, r.finalCost, r.iterations, r.evaluations, r.attempts,
        r.timeMs, static_cast<unsigned long long>(r.seed), r.repetition);
    return (n > 0) ? std::string(buf) : std::string();
}

bool writeExperimentCsv(const std::string& path,
                        const std::vector<ExperimentRecord>& records,
                        std::string* error) {
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        return error ? (*error = "E_CANT_WRITE_FILE", false) : false;
    }
    f << experimentCsvHeader() << '\n';
    for (const auto& r : records) {
        f << experimentCsvRow(r) << '\n';
    }
    f.flush();
    if (!f) {
        return error ? (*error = "E_CANT_FLUSH_FILE", false) : false;
    }
    return true;
}

} // namespace opcoda::opt
