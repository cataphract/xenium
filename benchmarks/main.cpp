#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "execution.hpp"

using boost::property_tree::ptree;
std::unordered_map<std::string, benchmark_builders> benchmarks;

void print_config(const ptree& config, int indent = 0) {
  if (config.empty()) {
    std::cout << config.get_value<std::string>() << '\n';
  } else {
    for (auto& it : config) {
      std::cout << std::string(2 * (indent + 1), ' ');
      std::cout << it.first << ": ";
      print_config(it.second, indent + 1);
    }
  }
}

bool config_matches(const ptree& config, const ptree& descriptor) {
  for (auto& entry : config) {
    auto it = descriptor.find(entry.first);
    if (it == descriptor.not_found()) {
      return false;
    }
    
    if (entry.second.empty()) {
      if (it->second.get_value<std::string>() == DYNAMIC_PARAM)
        continue;

      if (entry.second != it->second)
        return false;
    } else if (!config_matches(entry.second, it->second))
      return false;
  }
  return true;
}

class runner
{
public:
  runner(char* configfile);
  void run();

private:
  void load_config();
  std::shared_ptr<benchmark_builder> find_matching_builder(const benchmark_builders& benchmarks);

  ptree _config;
  std::shared_ptr<benchmark_builder> _builder;
};

runner::runner(char* configfile) {
  ptree config;
  std::ifstream stream(configfile);
  boost::property_tree::json_parser::read_json(stream, _config);
  load_config();
}

void runner::load_config() {
  auto benchmark_config = _config.get_child("benchmark");

  auto type = benchmark_config.get<std::string>("type");
  auto it = benchmarks.find(type);
  if (it == benchmarks.end())
    throw std::runtime_error("Invalid benchmark type " + type);
  
  _builder = find_matching_builder(it->second);
  if (!_builder) {
    std::cout << "Could not find a benchmark that matches the given configuration. Available configurations are:\n";
    for (auto& var : it->second) {
      print_config(var->get_descriptor());
      std::cout << '\n';
    }
    throw std::runtime_error("Invalid config");
  }
}

std::shared_ptr<benchmark_builder> runner::find_matching_builder(const benchmark_builders& benchmarks)
{
  auto& ds_config = _config.get_child("benchmark.ds");
  std::cout << "Given config:\n";
  print_config(ds_config);
  for(auto& var : benchmarks) {
    auto descriptor = var->get_descriptor();
    if (config_matches(ds_config, descriptor)) {
      std::cout << "Found matching benchmark:\n";
      print_config(descriptor);
      return var;
    }
  }
  return nullptr;
}

void runner::run() {
  assert(_builder != nullptr);
  auto threads = _config.get_child("threads");

  auto rounds = _config.get<std::uint32_t>("benchmark.rounds", 10);

  for (std::uint32_t i = 0; i < rounds; ++i) {
    std::cout << "round " << i << std::endl;
    auto benchmark = _builder->build();
    benchmark->setup(_config.get_child("benchmark"));
    execution exec(_config, benchmark);
    exec.run();
  }
}

void print_usage() {
  std::cout << "Usage: benchmark <config-file> | --help" << std::endl;
}

void print_available_benchmarks() {
  std::cout << "\nAvailable benchmark configurations:\n";
  for (auto& benchmark : benchmarks) {
    std::cout << "=== " << benchmark.first << " ===\n";
    for (auto& config : benchmark.second) 
      print_config(config->get_descriptor());
    std::cout << std::endl;
  }
  // TODO - improve output
}

int main (int argc, char* argv[])
{
#if !defined(NDEBUG)
  std::cout
    << "==============================\n"
    << "  This is a __DEBUG__ build!  \n"
    << "=============================="
    << std::endl;
#endif

  if (argc < 2) {
    print_usage();
    return 1;
  }

  if (argv[1] == std::string("--help")) {
    print_usage();
    print_available_benchmarks();
    return 0;
  }

  try
  {
    runner runner(argv[1]);
    runner.run();
    return 0;
  } catch (const std::exception& e) {
    std::cout << "ERROR: " << e.what() << std::endl;
    return 1;
  }
}
