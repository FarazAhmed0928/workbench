// CLI benchmark: bench [cases] [elements] [seed] [outdir]
#include <cstdio>
#include <cstdlib>
#include <fstream>

#include "benchmark.hpp"

int main(int argc, char** argv) {
    wb::BatchConfig cfg;
    cfg.cases = 20000;  // ~0.5 s serial on a modern core: long enough to time reliably
    if (argc > 1) cfg.cases = std::atoi(argv[1]);
    if (argc > 2) cfg.elements = std::atoi(argv[2]);
    if (argc > 3) cfg.seed = std::strtoull(argv[3], nullptr, 10);
    const std::string out = argc > 4 ? argv[4] : "results";
    const std::string err = wb::validate(cfg);
    if (!err.empty()) { std::fprintf(stderr, "%s\n", err.c_str()); return 2; }
    auto rep = wb::runBenchmark(cfg, 4, 1, 5);
    const std::string text = wb::benchText(rep);
    std::fputs(text.c_str(), stdout);
    std::ofstream(out + "/benchmark.csv") << wb::benchCsv(rep);
    std::ofstream(out + "/benchmark_summary.txt") << text;
    return rep.agreementOk ? 0 : 1;
}
