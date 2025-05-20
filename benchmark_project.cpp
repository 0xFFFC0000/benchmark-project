#include <boost/chrono/duration.hpp>
#include <boost/exception/exception.hpp>
#include <boost/filesystem/directory.hpp>
#include <boost/filesystem/file_status.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/json.hpp>
#include <boost/json/array.hpp>
#include <boost/json/kind.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/serializer.hpp>
#include <boost/json/value.hpp>
#include <boost/log/core.hpp>
#include <boost/log/core/core.hpp>
#include <boost/log/keywords/severity.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/process.hpp>
#include <boost/process/search_path.hpp>
#include <boost/process/spawn.hpp>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/pthread/condition_variable_fwd.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <boost/thread/pthread/thread_data.hpp>
#include <chrono>
#include <cpr/api.h>
#include <cpr/body.h>
#include <cpr/cpr.h>
#include <cpr/cprtypes.h>
#include <cpr/response.h>
#include <cpr/timeout.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <exception>
#include <format>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <utility>

namespace process = boost::process;
namespace filesystem = boost::filesystem;
namespace po = boost::program_options;

// LTRACE << "A trace severity message";
// LDEBUG << "A debug severity message";
// // LINFO << "An informational severity message";
// LWARNING << "A warning severity message";
// LERROR << "An error severity message";
// LFATAL << "A fatal severity message";

#define LTRACE BOOST_LOG_TRIVIAL(trace)
#define LDEBUG BOOST_LOG_TRIVIAL(debug)
#define LINFO BOOST_LOG_TRIVIAL(info)
#define LWARNING BOOST_LOG_TRIVIAL(warning)
#define LERROR BOOST_LOG_TRIVIAL(error)
#define LFATAL BOOST_LOG_TRIVIAL(fatal)

#define LDIE(msg, err)                                                                                   \
    {                                                                                                    \
        BOOST_LOG_TRIVIAL(fatal) << (msg) << " : " << __LINE__ << " : " << __FUNCTION__ << ". exiting."; \
        exit(err);                                                                                       \
    }

// Root directory containing all tests
std::string test_root;

// Numbers of wallets to run
uint32_t number_of_nodes;

// how long it should mine
uint32_t test_duration;

// how many times a cycle (open_wallet / refresh / close) should run.
uint32_t benchmark_iteration;

#define MAX_NUMBER_OF_NODES (1024 << 2)
#define MAX_test_duration (3600 * 24)
#define MAX_BENCHMARK_ITERATION_TIME 1024
#define UPDATE_INTERVAL 2
#define TIMEOUTMS UPDATE_INTERVAL * 1000

namespace WalletRPC {

std::mutex address_wallet_mutex;
std::map<size_t, std::string> address_wallet;

constexpr const int RPC_BASE_PORT = 20048;
constexpr const std::string RPC_DEFAULT_IP = "127.0.0.1";

const std::string& EXEC_NAME = "monero-wallet-rpc";
filesystem::path EXEC_PATH;

constexpr const char RPC_BIND_IP_CINTAINER[] = "--rpc-bind-ip={}";
constexpr const char RPC_BIND_PORT_CONTAINER[] = "--rpc-bind-port={}";
constexpr const char NO_INITIAL_SYNC[] = "--no-initial-sync";
constexpr const char MAX_CONCURRENCY_CONTAINER[] = "--max-concurrency={}";
constexpr const char TESTNET[] = "--testnet";
constexpr const char MAINNET[] = "--mainnet";
constexpr const char DAEMON_ADDRESS_CONTAINER[] = "--daemon-address={}:{}";
constexpr const char WALLET_DIR_CONTAINER[] = "--wallet-dir={}";
constexpr const char PASSWORD_CONTAINER[] = "--password={}";
constexpr const char LOG_FILE_CONTAINER[] = "--log-file={}";
constexpr const char LOG_LEVEL_CONTAINER[] = "--log-level={}";
constexpr const char MAX_LOG_FILE_SIZE_CONTAINER[] = "--max-log-file-size={}";
constexpr const char MAX_LOG_FILES_CONTAINER[] = "--max-log-files={}";
constexpr const char DAEMON_SSL_ALLOW_ANY_CERT[] = "--daemon-ssl-allow-any-cert";
constexpr const char DISABLE_RPC_LOGIN[] = "--disable-rpc-login";
constexpr const char RPC_MAX_CONNECTIONS_PER_PUBLIC_IP[] = "--rpc-max-connections-per-public-ip={}";
constexpr const char RPC_MAX_CONNECTIONS_PER_PRIVATE_IP[] = "--rpc-max-connections-per-private-ip={}";
constexpr const char RPC_MAX_CONNECTIONS[] = "--rpc-max-connections={}";
constexpr const char RPC_RESPONSE_SOFT_LIMIT[] = "--rpc-response-soft-limit={}";
constexpr const char SHARED_RINGDB_DIR_CONTAINER[] = "--shared-ringdb-dir={}";
constexpr const char TRUSTED_DAEMON[] = "--trusted-daemon";
constexpr const char NO_DNS[] = "--no-dns";
constexpr const char NON_INTERACTIVE[] = "--non-interactive";
constexpr const char RPC_SSL_CONTAINER[] = "--rpc-ssl=disabled";
constexpr const char DAEMON_SSL_CONTAINER[] = "--daemon-ssl=disabled";

struct walletrpc {
    std::string ip_address = RPC_DEFAULT_IP;

    size_t rpc_port;
    size_t max_concurrency = 4;
    size_t max_log_file_size = (1024 << 20); // 1 GB
    size_t max_log_files = 1;
    std::string daemon_ip_address;
    size_t daemon_port;
    std::string wallet_dir;
    std::string password;
    size_t log_level = 4;
    std::string shared_ringdb_dir;
    size_t rpc_max_connections_per_public_ip = 1024;
    size_t rpc_max_connections_per_private_ip = 1024;
    size_t rpc_max_connections = 1024 << 2; // 4096
    size_t rpc_response_soft_limit = 1024 << 20; // 1 GB
};
std::string Default_wallet_location()
{
    return test_root + filesystem::path::preferred_separator + "wallets";
}

std::string get_wallet_name_for_ith(int index)
{
    return "Wallet_" + std::to_string(index);
}

} // namespace WalletRPC

namespace Daemon {
const std::string& EXEC_NAME = "monerod";
filesystem::path TEST_EXEC_PATH;
filesystem::path MASTER_EXEC_PATH;

std::mutex daemons_pids_mutex;
std::map<size_t, pid_t> daemons_pids;

constexpr const int RPC_BASE_PORT = 4096;
constexpr const int P2P_BASE_PORT = 28081;
constexpr const std::string RPC_DEFAULT_IP = "127.0.0.1";

constexpr const char RPC_BIND_PORT_CONTAINER[] = "--rpc-bind-port={}";
constexpr const char RPC_BIND_IP_CONTAINER[] = "--rpc-bind-ip={}";
constexpr const char P2P_BIND_IP_CONTAINER[] = "--p2p-bind-ip={}";
constexpr const char P2P_BIND_PORT_CONTAINER[] = "--p2p-bind-port={}";
constexpr const char DATA_DIR_CONTAINER[] = "--data-dir={}";
constexpr const char ADD_EXCLUSIVE_NODE_CONTAINER[] = "--add-exclusive-node={}:{}";
constexpr const char DIFFICULTY_CONTAINER[] = "--fixed-difficulty={}";
constexpr const char MAX_CONCURRENCY_CONTAINER[] = "--max-concurrency={}";
constexpr const char NO_SYNC[] = "--no-sync";
constexpr const char NO_ZMQ[] = "--no-zmq";
constexpr const char OFFLINE[] = "--offline";
constexpr const char LOG_FILE_CONTAINER[] = "--log-file={}";
constexpr const char LOG_LEVEL_CONTAINER[] = "--log-level={}";
constexpr const char MAX_LOG_FILE_SIZE_CONTAINER[] = "--max-log-file-size={}";
constexpr const char MAX_LOG_FILES_CONTAINER[] = "--max-log-files={}";
constexpr const char RPC_MAX_CONNECTIONS_PER_PUBLIC_IP[] = "--rpc-max-connections-per-public-ip={}";
constexpr const char RPC_MAX_CONNECTIONS_PER_PRIVATE_IP[] = "--rpc-max-connections-per-private-ip={}";
constexpr const char RPC_MAX_CONNECTIONS[] = "--rpc-max-connections={}";
constexpr const char RPC_RESPONSE_SOFT_LIMIT[] = "--rpc-response-soft-limit={}";
constexpr const char LIMIT_RATE_UP[] = "--limit-rate-up={}";
constexpr const char LIMIT_RATE_DOWN[] = "--limit-rate-down={}";
constexpr const char TESTNET[] = "--testnet";
constexpr const char MAINNET[] = "--mainnet";
constexpr const char NON_INTERACTIVE[] = "--non-interactive";
constexpr const char START_MINING_CONTAINER[] = "--start-mining={}";
constexpr const char CONFIRM_EXTERNAL_BIND[] = "--confirm-external-bind";
constexpr const char ALLOW_LOCAL_IP[] = "--allow-local-ip";
constexpr const char DISABLE_RPC_BAN[] = "--disable-rpc-ban";
constexpr const char DISABLE_DNS_CHECKPOINTS[] = "--disable-dns-checkpoints";
constexpr const char RPC_SSL_ALLOW_ANY_CERT[] = "--rpc-ssl-allow-any-cert";
constexpr const char MAX_CONNECTIONS_PER_IP[] = "--max-connections-per-ip={}";
constexpr const char MINING_THREADS[] = "--mining-threads={}";
constexpr const char NO_IGD[] = "--no-igd";
constexpr const char HIDE_MY_PORT[] = "--hide-my-port";
constexpr const char BLOCK_SYNC_SIZE_CONTAINER[] = "--block-sync-size={}";
constexpr const char RPC_SSL_CONTAINER[] = "--rpc-ssl=disabled";
// Temporary
// constexpr const char CONFIG_FILE_CONTAINER[] = "--config-file={}";

struct daemon {
    filesystem::path exec_path;
    std::string ip_address = RPC_DEFAULT_IP;
    size_t log_level = 2;
    size_t max_log_file_size = (1024 << 15); // 32 MB
    size_t max_log_files = 1;
    size_t max_concurrency = 4;
    size_t max_connections_per_ip = (2048 << 10); // 2 MB
    size_t block_sync_size = 2048 << 16;
    size_t p2p_port;
    size_t mining_threads = 8;
    size_t rpc_port;
    size_t difficulty;
    std::string p2p_ip;
    std::string data_dir;
    std::vector<std::string> exclusive_nodes;
    size_t rpc_max_connections_per_public_ip = 1024;
    size_t rpc_max_connections_per_private_ip = 1024;
    size_t rpc_max_connections = 1024 << 2; // 4096
    size_t rpc_response_soft_limit = 1024 << 20; // 1 GB
    size_t limit_rate_up = 1024 << 20; // 1 GB
    size_t limit_rate_down = 1024 << 20; // 1 GB
};

std::string Default_daemon_location(int index)
{
    return test_root + filesystem::path::preferred_separator + "daemons" + filesystem::path::preferred_separator + std::to_string(index);
}
} // namespace Daemon

std::string log_location()
{
    return test_root + filesystem::path::preferred_separator + "logs" + filesystem::path::preferred_separator;
}

void run_wallet_rpc(WalletRPC::walletrpc& walletrpc, size_t index, boost::condition_variable& terminator)
{
    process::ipstream pipe_stream;
    process::child wallet_rpc_process(WalletRPC::EXEC_PATH,
        WalletRPC::NO_INITIAL_SYNC, WalletRPC::DISABLE_RPC_LOGIN, WalletRPC::TESTNET, WalletRPC::TRUSTED_DAEMON, WalletRPC::NO_DNS,
        WalletRPC::NON_INTERACTIVE, WalletRPC::RPC_SSL_CONTAINER, WalletRPC::DAEMON_SSL_CONTAINER,
        std::format(WalletRPC::DAEMON_ADDRESS_CONTAINER, walletrpc.daemon_ip_address, std::to_string(walletrpc.daemon_port)),
        std::format(WalletRPC::WALLET_DIR_CONTAINER, walletrpc.wallet_dir),
        std::format(WalletRPC::LOG_FILE_CONTAINER, log_location() + "wallet_" + std::to_string(index) + ".log.txt"),
        std::format(WalletRPC::LOG_LEVEL_CONTAINER, std::to_string(walletrpc.log_level)),
        std::format(WalletRPC::MAX_LOG_FILE_SIZE_CONTAINER, std::to_string(walletrpc.max_log_file_size)),
        std::format(WalletRPC::MAX_LOG_FILES_CONTAINER, std::to_string(walletrpc.max_log_files)),
        std::format(WalletRPC::SHARED_RINGDB_DIR_CONTAINER, walletrpc.shared_ringdb_dir),
        std::format(WalletRPC::MAX_CONCURRENCY_CONTAINER, std::to_string(walletrpc.max_concurrency)),
        std::format(WalletRPC::RPC_BIND_PORT_CONTAINER, std::to_string(walletrpc.rpc_port)),
        std::format(WalletRPC::RPC_MAX_CONNECTIONS_PER_PUBLIC_IP, std::to_string(walletrpc.rpc_max_connections_per_public_ip)),
        std::format(WalletRPC::RPC_MAX_CONNECTIONS_PER_PRIVATE_IP, std::to_string(walletrpc.rpc_max_connections_per_private_ip)),
        std::format(WalletRPC::RPC_MAX_CONNECTIONS, std::to_string(walletrpc.rpc_max_connections)),
        std::format(WalletRPC::RPC_RESPONSE_SOFT_LIMIT, std::to_string(walletrpc.rpc_response_soft_limit)),
        process::std_out > pipe_stream);

    std::string line;

    // wait for signal
    boost::mutex mutex;
    boost::unique_lock<boost::mutex> lock(mutex);
    terminator.wait(lock);

    LTRACE << "Terminating wallet.";
    wallet_rpc_process.terminate();
    if (wallet_rpc_process.joinable())
        wallet_rpc_process.join();
    return;
}

// void create_offline_daemon(Daemon::daemon& daemon, boost::condition_variable& terminator)
// {
//     if (!filesystem::exists(daemon.exec_path)) {
//         LDIE(daemon.exec_path.string() + " does not exist.", -1);
//     }

//     process::ipstream pipe_stream;

//     process::child daemon_process(daemon.exec_path,
//         Daemon::NO_SYNC, Daemon::OFFLINE, Daemon::NON_INTERACTIVE, Daemon::NO_ZMQ,
//         Daemon::RPC_SSL_CONTAINER,
//         std::format(Daemon::P2P_BIND_IP_CONTAINER, daemon.p2p_ip),
//         std::format(Daemon::P2P_BIND_PORT_CONTAINER, std::to_string(daemon.p2p_port)),
//         std::format(Daemon::RPC_BIND_PORT_CONTAINER, std::to_string(daemon.rpc_port)),
//         std::format(Daemon::MAX_CONCURRENCY_CONTAINER, std::to_string(daemon.max_concurrency)),
//         std::format(Daemon::DATA_DIR_CONTAINER, daemon.data_dir),
//         std::format(Daemon::LOG_FILE_CONTAINER, test_root + filesystem::path::preferred_separator + "wallet_creation_daemon.log.txt"),
//         std::format(Daemon::LOG_LEVEL_CONTAINER, std::to_string(daemon.log_level)),
//         process::std_out > pipe_stream);

//     std::string line;

//     // wait for signal
//     boost::mutex mutex;
//     boost::unique_lock<boost::mutex> lock(mutex);
//     terminator.wait(lock);

//     LTRACE << "Terminating daemon.";
//     daemon_process.terminate();
//     if (daemon_process.joinable())
//         daemon_process.join();
//     return;
// }

void set_log_level(const std::string& lvl)
{
    if (boost::iequals(lvl, L"trace"))
        boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::trace);
    else if (boost::iequals(lvl, L"debug"))
        boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::debug);
    else if (boost::iequals(lvl, L"info"))
        boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
    else if (boost::iequals(lvl, L"warning"))
        boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::warning);
    else if (boost::iequals(lvl, L"error"))
        boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::error);
    else if (boost::iequals(lvl, L"fatal"))
        boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::fatal);
    else
        boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::fatal);

    return;
}

void parse_and_validate(int argc, char** argv)
{
    po::options_description desc("Allowed options");

    desc.add_options()("test_builddir",
        po::value<std::string>(),
        "A directory that cointains test monero executables.");

    desc.add_options()("master_builddir",
        po::value<std::string>(),
        "A directory that cointains master (vanilla) monero executables.");

    desc.add_options()(
        "test_root",
        po::value<std::string>(),
        "A directory that will contain all the blockchains and wallets.");

    desc.add_options()("number_of_nodes",
        po::value<uint32_t>(),
        "Number of wallets that should be generated.");

    desc.add_options()("test_duration",
        po::value<uint32_t>(),
        "How long it should mine.");

    desc.add_options()("benchmark_iteration",
        po::value<uint32_t>(),
        "How mnay times cycle (open_wallet / refresh / close) should run.");

    desc.add_options()("log_level",
        po::value<std::string>(),
        "Log level. can be: trace, debug, info, warning, error, fatal or none.");

    desc.add_options()("help", "Print usage.");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << "Usage: " << argv[0] << " [options] <description of positional 1> <description of positional 2> ...\n";
        std::cout << desc;
        exit(0);
    }

    std::string test_builddir;
    if (vm.count("test_builddir")) {
        test_builddir = vm["test_builddir"].as<std::string>();
        if (!test_builddir.empty()
            && filesystem::exists(test_builddir)
            && filesystem::is_directory(test_builddir)) {
            // LINFO << "test_builddir is " << test_builddir;
            Daemon::TEST_EXEC_PATH = test_builddir + filesystem::path::preferred_separator + Daemon::EXEC_NAME;
        }
    } else {
        LDIE("test_builddir is not defined.", -1);
    }

    std::string master_builddir;
    if (vm.count("master_builddir")) {
        master_builddir = vm["master_builddir"].as<std::string>();
        if (!master_builddir.empty()
            && filesystem::exists(master_builddir)
            && filesystem::is_directory(master_builddir)) {
            // LINFO << "master_builddir is " << master_builddir;
            WalletRPC::EXEC_PATH = master_builddir + filesystem::path::preferred_separator + WalletRPC::EXEC_NAME;
            Daemon::MASTER_EXEC_PATH = master_builddir + filesystem::path::preferred_separator + Daemon::EXEC_NAME;
        }
    } else {
        LFATAL << "master_builddir is not defined. using default monero intalled file";
        WalletRPC::EXEC_PATH = process::search_path(WalletRPC::EXEC_NAME);
        Daemon::MASTER_EXEC_PATH = process::search_path(Daemon::EXEC_NAME);
        if (WalletRPC::EXEC_PATH.string().empty()
            || Daemon::MASTER_EXEC_PATH.string().empty()) {
            LDIE("Cannot find monero-wallet-rpc and/or monerod executable.", -1);
        }
        LFATAL << "Will use " << WalletRPC::EXEC_PATH << " as monero-wallet-rpc.";
        LFATAL << "Will use " << Daemon::MASTER_EXEC_PATH << " as monerod.";
    }

    test_root = filesystem::path(getenv("HOME")).string() + filesystem::path::preferred_separator + ".benchmark_project";
    if (vm.count("test_root")) {
        test_root = vm["test_root"].as<std::string>();
        if (!test_root.empty() && filesystem::exists(test_root) && filesystem::is_directory(test_root)) {
            LINFO << "test_root is " << test_root;
        } else if (!test_root.empty() && !filesystem::exists(test_root)) {
            filesystem::create_directory(test_root);
            LINFO << "test_root is " << test_root;
        }
    } else {
        LFATAL << "test_root is not defined.";
        LFATAL << "Will use " << test_root << " as test root directory.";
    }

    // force remove a directory
    if (filesystem::exists(test_root)) {
        try {
            LINFO << "Removing " << test_root;
            filesystem::remove_all(test_root);
        } catch (boost::filesystem::filesystem_error fe) {
            LFATAL << fe.what();
        }
    }

    // create same directory
    try {
        LINFO << "Creating " << test_root;
        filesystem::create_directory(test_root);
    } catch (boost::filesystem::filesystem_error fe) {
        LFATAL << fe.what();
    }

    if (vm.count("number_of_nodes")) {
        if (vm["number_of_nodes"].as<uint32_t>() < MAX_NUMBER_OF_NODES) {
            number_of_nodes = vm["number_of_nodes"].as<uint32_t>();
        } else {
            LDIE("number_of_nodes is invalid.", -1);
        }
    }

    test_duration = 30;
    if (vm.count("test_duration")) {
        if (vm["test_duration"].as<uint32_t>() < MAX_test_duration) {
            test_duration = vm["test_duration"].as<uint32_t>();

        } else {
            LDIE("test_duration is invalid.", -1);
        }
    }

    benchmark_iteration = 10;
    if (vm.count("benchmark_iteration")) {
        if (vm["benchmark_iteration"].as<uint32_t>() < MAX_BENCHMARK_ITERATION_TIME) {
            benchmark_iteration = vm["benchmark_iteration"].as<uint32_t>();

        } else {
            LDIE("benchmark_iteration is invalid.", -1);
        }
    }

    if (vm.count("log_level")) {
        set_log_level(vm["log_level"].as<std::string>());
    } else {
        set_log_level("info"); // default log level
    }

    LINFO << "test_duration is " << test_duration;
    LINFO << "number_of_nodes is " << number_of_nodes;
    LINFO << "benchmark_iteration is " << benchmark_iteration;
}

std::string generate_create_wallet_request(int index)
{
    // $ curl http://localhost:18082/json_rpc -d '{"jsonrpc":"2.0","id":"0","method":"create_wallet","params":{"filename":"mytestwallet","password":"mytestpassword","language":"English"}}' -H 'Content-Type: application/json'

    boost::json::key_value_pair filename("filename", ("Wallet_" + std::to_string(index)));
    boost::json::key_value_pair password("password", "''");
    boost::json::key_value_pair language("language", "English");
    boost::json::object param_list;
    param_list.insert(filename);
    param_list.insert(password);
    param_list.insert(language);
    boost::json::key_value_pair param({ "params", param_list });

    boost::json::key_value_pair jsonrpc("jsonrpc", "2.0");
    boost::json::key_value_pair method("method", "create_wallet");
    boost::json::key_value_pair id("id", "2.0");

    boost::json::object rpc_request;
    rpc_request.insert(jsonrpc);
    rpc_request.insert(id);
    rpc_request.insert(method);
    rpc_request.insert(param);
    std::string request_json = boost::json::serialize(rpc_request);
    LTRACE << "Sending JSON-RPC request : " << boost::json::serialize(rpc_request);
    return request_json;
}

std::string generate_open_wallet_request(int index)
{
    // $ curl http://localhost:18082/json_rpc -d '{"jsonrpc":"2.0","id":"0","method":"open_wallet","params":{"filename":"mytestwallet","password":"mytestpassword"}}' -H 'Content-Type: application/json'

    boost::json::key_value_pair filename("filename", WalletRPC::get_wallet_name_for_ith(index));
    boost::json::key_value_pair password("password", "''");
    boost::json::object param_list;
    param_list.insert(filename);
    param_list.insert(password);
    boost::json::key_value_pair param({ "params", param_list });

    boost::json::key_value_pair jsonrpc("jsonrpc", "2.0");
    boost::json::key_value_pair method("method", "open_wallet");
    boost::json::key_value_pair id("id", "2.0");

    boost::json::object rpc_request;
    rpc_request.insert(jsonrpc);
    rpc_request.insert(id);
    rpc_request.insert(method);
    rpc_request.insert(param);
    std::string request_json = boost::json::serialize(rpc_request);
    LTRACE << "Sending JSON-RPC request : " << boost::json::serialize(rpc_request);
    return request_json;
}

std::string generate_get_balance_request(size_t _account_index = 0, std::vector<size_t> _address_indices = { 0 })
{
    // $ curl http://127.0.0.1:18082/json_rpc -d '{"jsonrpc":"2.0","id":"0","method":"get_balance","params":{"account_index":0,"address_indices":[0,1]}}' -H 'Content-Type: application/json'
    boost::json::key_value_pair account_index("account_index", _account_index);
    boost::json::array address_indices_array;
    for (const auto& index : _address_indices) {
        address_indices_array.push_back(index);
    }
    boost::json::key_value_pair address_indices("address_indices", address_indices_array);
    boost::json::object param_list;
    param_list.insert(account_index);
    param_list.insert(address_indices);
    boost::json::key_value_pair param({ "params", param_list });

    boost::json::key_value_pair jsonrpc("jsonrpc", "2.0");
    boost::json::key_value_pair method("method", "get_balance");
    boost::json::key_value_pair id("id", "2.0");

    boost::json::object rpc_request;
    rpc_request.insert(jsonrpc);
    rpc_request.insert(id);
    rpc_request.insert(method);
    rpc_request.insert(param);
    std::string request_json = boost::json::serialize(rpc_request);
    LTRACE << "Sending JSON-RPC request : " << boost::json::serialize(rpc_request);
    return request_json;
}

std::string generate_refresh_request(std::size_t _start_height = 0)
{
    // $ curl http://localhost:18082/json_rpc -d '{"jsonrpc":"2.0","id":"0","method":"refresh","params":{"start_height":100000}}' -H 'Content-Type: application/json'

    boost::json::key_value_pair start_height("start_height", _start_height);
    boost::json::object param_list;
    param_list.insert(start_height);
    boost::json::key_value_pair param({ "params", param_list });

    boost::json::key_value_pair jsonrpc("jsonrpc", "2.0");
    boost::json::key_value_pair method("method", "refresh");
    boost::json::key_value_pair id("id", "2.0");

    boost::json::object rpc_request;
    rpc_request.insert(jsonrpc);
    rpc_request.insert(id);
    rpc_request.insert(method);
    rpc_request.insert(param);
    std::string request_json = boost::json::serialize(rpc_request);
    LTRACE << "Sending JSON-RPC request : " << boost::json::serialize(rpc_request);
    return request_json;
}

std::string generate_start_mining_request(std::size_t _threads_count = 4, bool _do_background_mining = false, bool _ignore_battery = true)
{
    // curl http://localhost:18082/json_rpc -d '{"jsonrpc":"2.0","id":"0","method":"start_mining","params":{"threads_count":1,"do_background_mining":true,"ignore_battery":false}}' -H 'Content-Type: application/json'
    boost::json::key_value_pair threads_count("threads_count", _threads_count);
    boost::json::key_value_pair do_background_mining("do_background_mining", _do_background_mining);
    boost::json::key_value_pair ignore_battery("ignore_battery", _ignore_battery);
    boost::json::object param_list;
    param_list.insert(threads_count);
    param_list.insert(do_background_mining);
    param_list.insert(ignore_battery);
    boost::json::key_value_pair param({ "params", param_list });

    boost::json::key_value_pair jsonrpc("jsonrpc", "2.0");
    boost::json::key_value_pair method("method", "start_mining");
    boost::json::key_value_pair id("id", "2.0");

    boost::json::object rpc_request;
    rpc_request.insert(jsonrpc);
    rpc_request.insert(id);
    rpc_request.insert(method);
    rpc_request.insert(param);
    std::string request_json = boost::json::serialize(rpc_request);
    LTRACE << "Sending JSON-RPC request : " << boost::json::serialize(rpc_request);
    return request_json;
}

std::string generate_get_address_request()
{
    // $ curl http://127.0.0.1:18082/json_rpc -d '{"jsonrpc":"2.0","id":"0","method":"get_address"' -H 'Content-Type: application/json'
    boost::json::key_value_pair jsonrpc("jsonrpc", "2.0");
    boost::json::key_value_pair method("method", "get_address");
    boost::json::key_value_pair id("id", "2.0");

    boost::json::object rpc_request;
    rpc_request.insert(jsonrpc);
    rpc_request.insert(id);
    rpc_request.insert(method);
    std::string request_json = boost::json::serialize(rpc_request);
    return request_json;
}

std::string generate_transfer_request(const std::vector<std::pair<std::string, std::string>>& _address_and_amount)
{
    // $ curl http://127.0.0.1:18082/json_rpc -d '{"jsonrpc":"2.0","id":"0","method":"transfer","params":{"destinations":[{"amount":100000000000,"address":"7BnERTpvL5MbCLtj5n9No7J5oE5hHiB3tVCK5cjSvCsYWD2WRJLFuWeKTLiXo5QJqt2ZwUaLy2Vh1Ad51K7FNgqcHgjW85o"}],"account_index":0, "priority":0,"ring_size": 7,"get_tx_hex":true}}' -H 'Content-Type: application/json'
    boost::json::object destination;

    std::for_each(_address_and_amount.begin(), _address_and_amount.end(),
        [&](const std::pair<std::string, std::string>& item) {
            destination.insert(boost::json::key_value_pair("address", item.first));
            destination.insert(boost::json::key_value_pair("amount", item.second));
        });

    boost::json::array destination_array;
    destination_array.push_back(destination);
    boost::json::key_value_pair destinations("destinations", destination_array);

    boost::json::key_value_pair account_index("account_index", 0);
    boost::json::key_value_pair priority("priority", 0);
    boost::json::key_value_pair ring_size("ring_size", 7);
    boost::json::key_value_pair get_tx_hex("get_tx_hex", true);
    boost::json::object param_list;
    param_list.insert(destinations);
    param_list.insert(account_index);
    param_list.insert(priority);
    param_list.insert(ring_size);
    param_list.insert(get_tx_hex);
    boost::json::key_value_pair param({ "params", param_list });

    boost::json::key_value_pair jsonrpc("jsonrpc", "2.0");
    boost::json::key_value_pair method("method", "transfer");
    boost::json::key_value_pair id("id", "2.0");

    boost::json::object rpc_request;
    rpc_request.insert(jsonrpc);
    rpc_request.insert(id);
    rpc_request.insert(method);
    rpc_request.insert(param);

    std::string request_json = boost::json::serialize(rpc_request);
    LTRACE << "Sending JSON-RPC request : " << boost::json::serialize(rpc_request);
    return request_json;
}

std::string generate_close_wallet_request()
{
    // $ curl http://localhost:18082/json_rpc -d '{"jsonrpc":"2.0","id":"0","method":"close_wallet"}' -H 'Content-Type: application/json'

    boost::json::key_value_pair jsonrpc("jsonrpc", "2.0");
    boost::json::key_value_pair method("method", "close_wallet");
    boost::json::key_value_pair id("id", "2.0");

    boost::json::object rpc_request;
    rpc_request.insert(jsonrpc);
    rpc_request.insert(id);
    rpc_request.insert(method);
    std::string request_json = boost::json::serialize(rpc_request);
    LTRACE << "Sending JSON-RPC request : " << boost::json::serialize(rpc_request);
    return request_json;
}

std::string generate_get_block_template_request(const std::string& _wallet_address, const std::size_t _reserve_size = 60)
{
    // $ curl http://127.0.0.1:18081/json_rpc -d '{"jsonrpc":"2.0","id":"0","method":"get_block_template","params":{"wallet_address":"44GBHzv6ZyQdJkjqZje6KLZ3xSyN1hBSFAnLP6EAqJtCRVzMzZmeXTC2AHKDS9aEDTRKmo6a6o9r9j86pYfhCWDkKjbtcns","reserve_size":60}}' -H 'Content-Type: application/json'

    boost::json::key_value_pair wallet_address("wallet_address", _wallet_address);
    boost::json::key_value_pair reserve_size("reserve_size", _reserve_size);
    boost::json::object param_list;
    param_list.insert(wallet_address);
    param_list.insert(reserve_size);
    boost::json::key_value_pair param({ "params", param_list });
    boost::json::key_value_pair jsonrpc("jsonrpc", "2.0");
    boost::json::key_value_pair method("method", "get_block_template");
    boost::json::key_value_pair id("id", "2.0");
    boost::json::object rpc_request;
    rpc_request.insert(jsonrpc);
    rpc_request.insert(id);
    rpc_request.insert(method);
    rpc_request.insert(param);
    std::string request_json = boost::json::serialize(rpc_request);
    LTRACE << "Sending JSON-RPC request : " << boost::json::serialize(rpc_request);
    return request_json;
}

std::string generate_submit_block_request(const std::string& _block_blob)
{
    // $ curl http://127.0.0.1:18081/json_rpc -d '{"jsonrpc":"2.0","id":"0","method":"submit_block","params":["0707e6bdfedc053771512f1bc27c62731ae9e8f2443db64ce742f4e57f5cf8d393de28551e441a0000000002fb830a01ffbf830a018cfe88bee283060274c0aae2ef5730e680308d9c00b6da59187ad0352efe3c71d36eeeb28782f29f2501bd56b952c3ddc3e350c2631d3a5086cac172c56893831228b17de296ff4669de020200000000"]}' -H 'Content-Type: application/json'

    boost::json::string block_blob(_block_blob);
    boost::json::array blob_array;
    blob_array.push_back(block_blob);
    boost::json::key_value_pair param({ "params", blob_array });
    boost::json::key_value_pair jsonrpc("jsonrpc", "2.0");
    boost::json::key_value_pair method("method", "submit_block");
    boost::json::key_value_pair id("id", "2.0");
    boost::json::object rpc_request;
    rpc_request.insert(jsonrpc);
    rpc_request.insert(id);
    rpc_request.insert(method);
    rpc_request.insert(param);
    std::string request_json = boost::json::serialize(rpc_request);
    LTRACE << "Sending JSON-RPC request : " << boost::json::serialize(rpc_request);
    return request_json;
}

std::string generate_sweep_dust()
{
    // curl http://127.0.0.1:18082/json_rpc -d '{"jsonrpc":"2.0","id":"0","method":"sweep_dust"}' -H 'Content-Type: application/json'
    boost::json::key_value_pair jsonrpc("jsonrpc", "2.0");
    boost::json::key_value_pair method("method", "sweep_dust");
    boost::json::key_value_pair id("id", "2.0");

    boost::json::object rpc_request;
    rpc_request.insert(jsonrpc);
    rpc_request.insert(id);
    rpc_request.insert(method);
    std::string request_json = boost::json::serialize(rpc_request);
    LTRACE << "Sending JSON-RPC request : " << boost::json::serialize(rpc_request);
    return request_json;
}

void run_wallets(std::atomic<bool>& wallet_stop_condition, std::size_t number_of_nodes, std::size_t test_duration)
{
    filesystem::create_directory(WalletRPC::Default_wallet_location());

    std::this_thread::sleep_for(std::chrono::seconds(10));

    boost::condition_variable wallet_terminator;
    auto wallet_rpc_creator = [&](size_t index) {
        WalletRPC::walletrpc walletrpc;
        walletrpc.rpc_port = WalletRPC::RPC_BASE_PORT + index;
        walletrpc.password = "''";
        walletrpc.daemon_port = Daemon::RPC_BASE_PORT + index;
        walletrpc.ip_address = walletrpc.daemon_ip_address = WalletRPC::RPC_DEFAULT_IP;
        walletrpc.log_level = 0;
        walletrpc.wallet_dir = WalletRPC::Default_wallet_location();
        walletrpc.shared_ringdb_dir = WalletRPC::Default_wallet_location();
        std::thread wallet_rpc_thread(run_wallet_rpc, std::ref(walletrpc), index, std::ref(wallet_terminator));
        if (wallet_rpc_thread.joinable())
            wallet_rpc_thread.join();
    };

    std::vector<std::thread> wallet_rpc_jobs;
    for (int index = 0; index < number_of_nodes; ++index) {
        wallet_rpc_jobs.push_back(std::thread(wallet_rpc_creator, index));
    }

    // std::this_thread::sleep_for(std::chrono::seconds(number_of_nodes > 10 ? number_of_nodes : 10));
    std::this_thread::sleep_for(std::chrono::seconds(15));

    auto wallet_creater_call = [&](int index) {
        std::string request_json = generate_create_wallet_request(index);
        cpr::Url url { std::format("http://{}:{}/json_rpc", WalletRPC::RPC_DEFAULT_IP, WalletRPC::RPC_BASE_PORT + index) };
        cpr::Header header { { "'Content-Type", "application/json" } };
        cpr::Response response;

        long status_code = 0;
        do {
            response = cpr::Post(url, header, cpr::Body { request_json }, cpr::Timeout(TIMEOUTMS * 10));
            status_code = response.status_code;
            if (response.status_code == 200) {
                LTRACE << "W" << index << " created successfully.";
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(UPDATE_INTERVAL));
        } while (status_code != 200);

        // call get address and save the wallet address
        request_json = generate_get_address_request();
        response = cpr::Post(url, header, cpr::Body { request_json }, cpr::Timeout(TIMEOUTMS));
        status_code = response.status_code;
        try {
            auto parsed = boost::json::parse(response.text);
            std::string address = value_to<std::string>(parsed.at("result").at("address"));
            std::lock_guard<std::mutex> lock(WalletRPC::address_wallet_mutex);
            WalletRPC::address_wallet.insert(std::make_pair(index, address));
            // LINFO << value_to<std::string>(parsed.at("result").at("address"));
        } catch (std::exception& e) {
            LFATAL << "Failed with error: ";
            LDIE(e.what(), -1);
        }
    };

    std::vector<std::thread> wallet_creator_jobs;
    wallet_creator_jobs.clear();
    for (int index = 0; index < number_of_nodes; ++index) {
        wallet_creator_jobs.push_back(std::thread(wallet_creater_call, index));
    }

    for (int index = 0; index < number_of_nodes; ++index) {
        if (wallet_creator_jobs.at(index).joinable())
            wallet_creator_jobs.at(index).join();
    }

    std::for_each(begin(WalletRPC::address_wallet), end(WalletRPC::address_wallet),
        [&](const std::pair<int, std::string>& item) {
            LTRACE << "W" << item.first << " : " << item.second;
        });

    std::this_thread::sleep_for(std::chrono::seconds(25));

    // mining step is disabled
    // auto wallet_start_mining = [&](size_t index) {
    //     std::string request_json = generate_start_mining_request(8, false, true);
    //     cpr::Url url { std::format("http://{}:{}/json_rpc", WalletRPC::RPC_DEFAULT_IP, WalletRPC::RPC_BASE_PORT + index) };
    //     cpr::Header header { { "'Content-Type", "application/json" } };

    //     long status_code = 0;
    //     do {
    //         cpr::Response response = cpr::Post(url, header, cpr::Body { request_json }, cpr::Timeout(TIMEOUTMS));
    //         status_code = response.status_code;
    //         if (response.status_code == 200) {
    //             LINFO << "W" << index << " started mining successfully";
    //             break;
    //         }
    //         std::this_thread::sleep_for(std::chrono::seconds(UPDATE_INTERVAL));
    //     } while (status_code != 200);
    // };

    // std::vector<std::thread> wallet_start_mining_jobs;
    // wallet_start_mining_jobs.clear();
    // for (int index = 0; index < number_of_nodes; ++index) {
    //     wallet_start_mining_jobs.push_back(std::thread(wallet_start_mining, index));
    // }

    // for (int index = 0; index < number_of_nodes; ++index) {
    //     if (wallet_start_mining_jobs.at(index).joinable())
    //         wallet_start_mining_jobs.at(index).join();
    // }

    // std::this_thread::sleep_for(std::chrono::seconds(10));

    auto wallet_transfer = [&](std::atomic<bool>& wallet_transfer_stop_condition, size_t index) {
        long status_code = 0;
        while (!wallet_transfer_stop_condition.load()) {
            // std::this_thread::sleep_for(std::chrono::milliseconds((rand() % UPDATE_INTERVAL)  * 100));
            if (rand() % 2) { // 50% chance to send money
                std::this_thread::sleep_for(std::chrono::seconds(rand() % UPDATE_INTERVAL));
                continue;
            }
            LTRACE << "W" << index << " is running a transfer";

            cpr::Url url { std::format("http://{}:{}/json_rpc", WalletRPC::RPC_DEFAULT_IP, WalletRPC::RPC_BASE_PORT + index) };
            cpr::Header header { { "'Content-Type", "application/json" } };

            // sweep dust
            std::string request_json = generate_sweep_dust();
            cpr::Response response = cpr::Post(url, header, cpr::Body { request_json }, cpr::Timeout(TIMEOUTMS));
            status_code = response.status_code;
            if (response.status_code != 200) {
                LTRACE << "W" << index << " address : " << WalletRPC::address_wallet[index] << " is unresponsive.";
                std::this_thread::sleep_for(std::chrono::seconds(rand() % UPDATE_INTERVAL));
                continue;
            }

            // call get balance
            request_json = generate_get_balance_request();
            response = cpr::Post(url, header, cpr::Body { request_json }, cpr::Timeout(TIMEOUTMS));
            if (response.status_code != 200) {
                LTRACE << "W" << index << " address : " << WalletRPC::address_wallet[index] << " is unresponsive.";
                std::this_thread::sleep_for(std::chrono::seconds(rand() % UPDATE_INTERVAL));
                continue;
            }
            // balance
            auto parsed_balance = boost::json::parse(response.text);
            std::size_t unlocked_balance = value_to<std::size_t>(parsed_balance.at("result").at("unlocked_balance"));

            if (!unlocked_balance) {
                LTRACE << "W" << index << " address : " << WalletRPC::address_wallet[index] << " has no unlocked balance.";
                std::this_thread::sleep_for(std::chrono::seconds(rand() % UPDATE_INTERVAL));
                continue;
            }

            // pick target wallet address randomly
            std::string address = WalletRPC::address_wallet[rand() % number_of_nodes];
            // pick amount randomly
            // Atomic Units refer to the smallest fraction of 1 XMR.
            // One atomic unit is currently 1e-12 XMR (0.000000000001 XMR, or one piconero).
            // It may be changed in the future.
            std::string amount = std::to_string(((rand() % (unlocked_balance / 10))));
            // std::string amount = std::to_string(((rand() % 5) + 1) * 100000); // 0.001 XMR = 1000000000 atomic units
            // std::string amount = std::to_string(((rand() % 5) + 1) * 1000000); // 0.01 XMR = 1000000000 atomic units
            // std::string amount = std::to_string(((rand() % 5) + 1) * 10000000); // 0.1 XMR = 100000000 atomic units
            // std::string amount = std::to_string(((rand() % 5) + 1) * 100000000); // 0.1 XMR = 100000000 atomic units
            // std::string amount = std::to_string(((rand() % 5) + 1) * 1000000000); // 1 XMR = 1000000000 atomic units
            // std::string amount = std::to_string(((rand() % 5) + 1) * 10000000000); // 10 XMR = 10000000000 atomic units
            request_json = generate_transfer_request({ std::make_pair(address, amount) });
            response = cpr::Post(url, header, cpr::Body { request_json }, cpr::Timeout(TIMEOUTMS));
            status_code = response.status_code;
            // transfer failed
            // check parsed json to see has any error
            if (response.status_code == 200) {
                auto parsed = boost::json::parse(response.text);
                if (parsed.as_object().contains("error")) {
                    LTRACE << "W" << index << " send money to " << address << " with amount " << amount << " failed, " << " reported unlocked balance: " << unlocked_balance << " , error: " << parsed.at("error");
                    LTRACE << "response : " << response.text;
                } else {
                    LTRACE << "W" << index << " send money to " << address << " with amount " << amount << " successfully";
                }
            }
        }
    };

    std::atomic<bool> wallet_transfer_stop_condition = false;
    std::vector<std::thread> wallet_transfer_jobs;
    wallet_transfer_jobs.clear();
    for (int index = 0; index < number_of_nodes; ++index) {
        wallet_transfer_jobs.push_back(std::thread(wallet_transfer, std::ref(wallet_transfer_stop_condition), index));
    }

    for (int index = 0; index < number_of_nodes; ++index) {
        if (wallet_transfer_jobs.at(index).joinable())
            wallet_transfer_jobs.at(index).join();
    }

    // wait
    std::this_thread::sleep_for(std::chrono::seconds(test_duration));

    wallet_terminator.notify_all();

    for (int index = 0; index < number_of_nodes; ++index) {
        if (wallet_rpc_jobs.at(index).joinable())
            wallet_rpc_jobs.at(index).join();
    }
}

void run_daemon(Daemon::daemon& daemon, size_t index, size_t seconds)
{

    process::ipstream pipe_stream;

    std::vector<std::string> exclusive_nodes;

    for (int i = 0; i < number_of_nodes; ++i) {
        if (i == index)
            continue;
        exclusive_nodes.push_back(
            std::format(
                Daemon::ADD_EXCLUSIVE_NODE_CONTAINER,
                daemon.ip_address,
                std::to_string(Daemon::P2P_BASE_PORT + i)));
    }

    process::child daemon_process(daemon.exec_path,
        Daemon::NON_INTERACTIVE,
        Daemon::RPC_SSL_ALLOW_ANY_CERT,
        Daemon::NO_ZMQ, Daemon::NO_IGD, Daemon::TESTNET, // Daemon::HIDE_MY_PORT
        Daemon::DISABLE_RPC_BAN, Daemon::CONFIRM_EXTERNAL_BIND, Daemon::ALLOW_LOCAL_IP,
        Daemon::DISABLE_DNS_CHECKPOINTS, Daemon::RPC_SSL_CONTAINER,
        std::format(Daemon::MAX_CONNECTIONS_PER_IP, daemon.max_connections_per_ip),
        std::format(Daemon::RPC_BIND_IP_CONTAINER, daemon.ip_address),
        std::format(Daemon::P2P_BIND_IP_CONTAINER, daemon.p2p_ip),
        std::format(Daemon::DIFFICULTY_CONTAINER, daemon.difficulty),
        std::format(Daemon::P2P_BIND_PORT_CONTAINER, std::to_string(daemon.p2p_port)),
        std::format(Daemon::RPC_BIND_PORT_CONTAINER, std::to_string(daemon.rpc_port)),
        std::format(Daemon::MAX_CONCURRENCY_CONTAINER, std::to_string(daemon.max_concurrency)),
        std::format(Daemon::DATA_DIR_CONTAINER, daemon.data_dir),
        std::format(Daemon::LOG_FILE_CONTAINER, log_location() + "daemon_" + std::to_string(index) + ".log.txt"),
        std::format(Daemon::LOG_LEVEL_CONTAINER, std::to_string(daemon.log_level)),
        std::format(Daemon::MAX_LOG_FILE_SIZE_CONTAINER, std::to_string(daemon.max_log_file_size)),
        std::format(Daemon::MAX_LOG_FILES_CONTAINER, std::to_string(daemon.max_log_files)),
        std::format(Daemon::RPC_MAX_CONNECTIONS_PER_PUBLIC_IP, std::to_string(daemon.rpc_max_connections_per_public_ip)),
        std::format(Daemon::RPC_MAX_CONNECTIONS_PER_PRIVATE_IP, std::to_string(daemon.rpc_max_connections_per_private_ip)),
        std::format(Daemon::RPC_MAX_CONNECTIONS, std::to_string(daemon.rpc_max_connections)),
        std::format(Daemon::RPC_RESPONSE_SOFT_LIMIT, std::to_string(daemon.rpc_response_soft_limit)),
        std::format(Daemon::LIMIT_RATE_UP, daemon.limit_rate_up),
        std::format(Daemon::LIMIT_RATE_DOWN, daemon.limit_rate_down),
        // std::format(Daemon::MINING_THREADS, daemon.mining_threads),
        // std::format(Daemon::START_MINING_CONTAINER, WalletRPC::address_wallet.at(index)),
        exclusive_nodes,
        process::std_out > pipe_stream);

    {
        std::lock_guard<std::mutex> lock(Daemon::daemons_pids_mutex);
        Daemon::daemons_pids.insert(std::make_pair(index, daemon_process.id()));
    }

    std::string line;

    std::ofstream null_stream("/dev/null");
    while (pipe_stream && std::getline(pipe_stream, line) && !line.empty()) {
        // LERROR << "MINER : " << line;
        // redirect log into /dev/null
        null_stream << line;
    };
    null_stream.close();

    // wait
    std::this_thread::sleep_for(std::chrono::seconds(seconds));

    daemon_process.terminate();
    if (daemon_process.joinable())
        daemon_process.join();
    return;
}

void run_daemons(std::atomic<bool>& daemon_stop_condition, std::size_t number_of_nodes, std::size_t test_duration)
{
    auto miner_creator = [&](size_t index) {
        Daemon::daemon daemon;
        daemon.p2p_port = Daemon::P2P_BASE_PORT + index;
        daemon.rpc_port = Daemon::RPC_BASE_PORT + index;
        // daemon.mining_threads = 2 + (index / 3);
        daemon.difficulty = 1;
        daemon.ip_address = daemon.p2p_ip = WalletRPC::RPC_DEFAULT_IP;
        daemon.log_level = 2;
        daemon.data_dir = Daemon::Default_daemon_location(index);
        daemon.exec_path = Daemon::MASTER_EXEC_PATH;
        std::thread daemon_thread(run_daemon, std::ref(daemon), index, test_duration);
        if (daemon_thread.joinable())
            daemon_thread.join();
    };

    std::vector<std::thread> daemon_miner_jobs;
    for (int index = 0; index < number_of_nodes; ++index) {
        daemon_miner_jobs.push_back(std::thread(miner_creator, index));
    }

    std::this_thread::sleep_for(std::chrono::seconds(45));

    // mimick mining with get_block_template and submit_block
    auto mimick_mining = [&](std::atomic<bool>& daemon_stop_condition, size_t index) {
        do {
            LTRACE << "W" << index << " address is empty, skip";
            std::this_thread::sleep_for(std::chrono::seconds(UPDATE_INTERVAL));
        } while (WalletRPC::address_wallet[index].empty());

        while (!daemon_stop_condition.load()) {
            // std::this_thread::sleep_for(std::chrono::milliseconds((rand() % UPDATE_INTERVAL)  * 100));

            if (rand() % 200) { // 1% chance to get block template
                std::this_thread::sleep_for(std::chrono::seconds(((rand() % UPDATE_INTERVAL))));
                continue;
            }
            std::string block_blob;
            std::string request_json = generate_get_block_template_request(WalletRPC::address_wallet[index]);
            cpr::Url url { std::format("http://{}:{}/json_rpc", Daemon::RPC_DEFAULT_IP, Daemon::RPC_BASE_PORT + index) };
            cpr::Header header { { "'Content-Type", "application/json" } };

            long status_code = 0;
            cpr::Response response = cpr::Post(url, header, cpr::Body { request_json }, cpr::Timeout(TIMEOUTMS));
            status_code = response.status_code;
            if (response.status_code == 200) {
                LTRACE << "W" << index << " mined successfully";
                // save blocktemplate_blob into block_blob
                auto parsed = boost::json::parse(response.text);
                if (parsed.as_object().contains("error")) {
                    LTRACE << "W" << index << " has error: " << parsed.at("error");
                    continue;
                }
                block_blob = value_to<std::string>(parsed.at("result").at("blocktemplate_blob"));
                LTRACE << "block_blob : " << block_blob;
            } else {
                std::this_thread::sleep_for(std::chrono::seconds(((rand() % UPDATE_INTERVAL))));
                continue;
            }

            // submit block
            // std::this_thread::sleep_for(std::chrono::milliseconds((rand() % UPDATE_INTERVAL)  * 100));
            request_json = generate_submit_block_request(block_blob);
            response = cpr::Post(url, header, cpr::Body { request_json }, cpr::Timeout(TIMEOUTMS));
            status_code = response.status_code;
            if (response.status_code == 200) {
                LTRACE << "W" << index << " submitted successfully";
                LTRACE << "block_blob : " << block_blob;
            }
        }
    };

    std::vector<std::thread> mimick_mining_jobs;
    mimick_mining_jobs.clear();
    for (int index = 0; index < number_of_nodes; ++index) {
        mimick_mining_jobs.push_back(std::thread(mimick_mining, std::ref(daemon_stop_condition), index));
    }

    for (int index = 0; index < number_of_nodes; ++index) {
        if (mimick_mining_jobs.at(index).joinable())
            mimick_mining_jobs.at(index).join();
    }

    std::this_thread::sleep_for(std::chrono::seconds(10));

    for (int index = 0; index < number_of_nodes; ++index) {
        if (daemon_miner_jobs.at(index).joinable())
            daemon_miner_jobs.at(index).join();
    }
}

void delete_wallet_cache_for_ith(size_t index)
{
    LTRACE << "Delete cache for Wallet " << index;
    filesystem::path cache_to_remove = WalletRPC::Default_wallet_location() + filesystem::path::preferred_separator + WalletRPC::get_wallet_name_for_ith(index);
    if (filesystem::exists(cache_to_remove)) {
        filesystem::remove(cache_to_remove);
    }
}

void delete_wallet_caches()
{
    std::for_each(begin(WalletRPC::address_wallet), end(WalletRPC::address_wallet),
        [&](const std::pair<size_t, std::string>& item) {
            delete_wallet_cache_for_ith(item.first);
        });
}

// std::chrono::milliseconds do_benchmark(filesystem::path daemon_exec_path)
// {
//     Daemon::daemon daemon;
//     daemon.ip_address = daemon.p2p_ip = Daemon::RPC_DEFAULT_IP;
//     daemon.p2p_port = Daemon::P2P_BASE_PORT;
//     daemon.rpc_port = Daemon::RPC_BASE_PORT;
//     daemon.data_dir = Daemon::Default_daemon_location(1);
//     daemon.exec_path = daemon_exec_path;
//     daemon.log_level = 0;
//     boost::condition_variable daemon_terminator;
//     std::thread daemon_thread(create_offline_daemon, std::ref(daemon), std::ref(daemon_terminator));

//     if (!filesystem::exists(WalletRPC::EXEC_PATH)) {
//         LDIE(WalletRPC::EXEC_PATH.string() + " does not exist.", -1);
//     }

//     boost::condition_variable wallet_terminator;
//     auto wallet_rpc_creator = [&](int index) {
//         WalletRPC::walletrpc walletrpc;
//         walletrpc.rpc_port = WalletRPC::RPC_BASE_PORT + index;
//         walletrpc.password = "''";
//         walletrpc.daemon_port = Daemon::RPC_BASE_PORT;
//         walletrpc.ip_address = walletrpc.daemon_ip_address = WalletRPC::RPC_DEFAULT_IP;
//         walletrpc.log_level = 0;
//         walletrpc.max_concurrency = 128;
//         walletrpc.wallet_dir = WalletRPC::Default_wallet_location();
//         walletrpc.shared_ringdb_dir = WalletRPC::Default_wallet_location();
//         std::thread wallet_rpc_thread(run_wallet_rpc, std::ref(walletrpc), std::ref(wallet_terminator));
//         if (wallet_rpc_thread.joinable())
//             wallet_rpc_thread.join();
//     };

//     std::vector<std::thread> wallet_rpc_jobs;
//     for (int index = 0; index < number_of_nodes; ++index) {
//         wallet_rpc_jobs.push_back(std::thread(wallet_rpc_creator, index));
//     }

//     // std::this_thread::sleep_for(std::chrono::seconds(number_of_nodes > 10 ? number_of_nodes : 10));
//     std::this_thread::sleep_for(std::chrono::seconds(10));

//     auto wallet_benchmark = [&](int max_iter, int index) {
//         for (int iter = 0; iter < max_iter; ++iter) {
//             std::string request_json = generate_open_wallet_request(index);
//             cpr::Url url { std::format("http://{}:{}/json_rpc", WalletRPC::RPC_DEFAULT_IP, WalletRPC::RPC_BASE_PORT + index) };
//             cpr::Header header { { "'Content-Type", "application/json" } };

//             long status_code = 0;
//             // do {
//             cpr::Response response = cpr::Post(url, header, cpr::Body { request_json });
//             status_code = response.status_code;
//             if (status_code == 200) {
//                 LTRACE << "Wallet " << std::to_string(index) << " opened.";
//             } else if (response.status_code != 200) {
//                 LTRACE << "Wallet incorrect  " << response.status_code << " as status_code for wallet open : " << std::to_string(index);
//             }
//             // } while (status_code != 200);

//             // call get address and save the wallet address
//             request_json = generate_check_balance_request();
//             response = cpr::Post(url, header, cpr::Body { request_json });
//             status_code = response.status_code;
//             if (status_code == 200) {
//                 LTRACE << "Wallet " << std::to_string(index) << " get_balance.";
//             } else {
//                 LTRACE << "Wallet incorrect  " << response.status_code << " as status_code for wallet get_balance : " << std::to_string(index);
//             }

//             // call get address and save the wallet address
//             request_json = generate_refresh_request();
//             response = cpr::Post(url, header, cpr::Body { request_json });
//             status_code = response.status_code;
//             if (status_code == 200) {
//                 LTRACE << "Wallet " << std::to_string(index) << " refreshed.";
//             } else {
//                 LTRACE << "Wallet incorrect  " << response.status_code << " as status_code for wallet refresh : " << std::to_string(index);
//             }

//             // call get address and save the wallet address
//             request_json = generate_check_balance_request();
//             response = cpr::Post(url, header, cpr::Body { request_json });
//             status_code = response.status_code;
//             if (status_code == 200) {
//                 LTRACE << "Wallet " << std::to_string(index) << " get_balance.";
//             } else {
//                 LTRACE << "Wallet incorrect  " << response.status_code << " as status_code for wallet get_balance : " << std::to_string(index);
//             }

//             // call get address and save the wallet address
//             request_json = generate_close_wallet_request();
//             response = cpr::Post(url, header, cpr::Body { request_json });
//             status_code = response.status_code;
//             if (status_code == 200) {
//                 LTRACE << "Wallet " << std::to_string(index) << " closed.";
//             } else {
//                 LTRACE << "Wallet incorrect  " << response.status_code << " as status_code for wallet close : " << std::to_string(index);
//             }

//             delete_wallet_cache_for_ith(index);
//         }
//     };

//     std::vector<std::thread> warm_up_jobs;
//     warm_up_jobs.clear();
//     for (int index = 0; index < number_of_nodes; ++index) {
//         warm_up_jobs.push_back(std::thread(wallet_benchmark, 1, index));
//     }

//     for (int index = 0; index < number_of_nodes; ++index) {
//         if (warm_up_jobs.at(index).joinable())
//             warm_up_jobs.at(index).join();
//     }

//     std::this_thread::sleep_for(std::chrono::seconds(10));

//     auto start = std::chrono::steady_clock::now();
//     LINFO << "Run benchmarks...";

//     std::vector<std::thread> wallet_creator_jobs;
//     wallet_creator_jobs.clear();
//     for (int index = 0; index < number_of_nodes; ++index) {
//         wallet_creator_jobs.push_back(std::thread(wallet_benchmark, benchmark_iteration, index));
//     }

//     for (int index = 0; index < number_of_nodes; ++index) {
//         if (wallet_creator_jobs.at(index).joinable())
//             wallet_creator_jobs.at(index).join();
//     }

//     wallet_terminator.notify_all();
//     daemon_terminator.notify_all();

//     for (int index = 0; index < number_of_nodes; ++index) {
//         if (wallet_rpc_jobs.at(index).joinable())
//             wallet_rpc_jobs.at(index).join();
//     }

//     auto end = std::chrono::steady_clock::now();
//     auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
//     int benchmark_seconds = (elapsed / 1000).count();
//     int benchmark_miliseconds = (elapsed % 1000).count();
//     LINFO
//         << ">>> Total elapsed time for running the benchmark: " << benchmark_seconds << "." << benchmark_miliseconds;

//     if (daemon_thread.joinable())
//         daemon_thread.join();
//     return elapsed;
// }

void status_reporter(std::atomic<bool>& stop_condition, std::size_t number_of_nodes)
{
    // check if the node is running
    while (!stop_condition) {
        std::this_thread::sleep_for(std::chrono::seconds(UPDATE_INTERVAL));
        LINFO << "======================================================================";
        size_t number_of_active_nodes = 0;
        for (std::size_t index = 0; index < number_of_nodes; ++index) {
            // std::this_thread::sleep_for(std::chrono::milliseconds((rand() % UPDATE_INTERVAL)  * 10));
            cpr::Url url { std::format("http://{}:{}/get_info", Daemon::RPC_DEFAULT_IP, Daemon::RPC_BASE_PORT + index) };
            cpr::Header header { { "Content-Type", "application/json" } };
            cpr::Response response = cpr::Get(url, header, cpr::Timeout(5000), cpr::Timeout(TIMEOUTMS));
            if (response.status_code != 200) {
                LTRACE << "N" << index << " is unresponsive.";
                continue;
            }
            // Get Json and Parse it
            // status
            auto parsed = boost::json::parse(response.text);
            std::string status = value_to<std::string>(parsed.at("status"));
            // height
            auto parsed_height = boost::json::parse(response.text);
            std::size_t height = value_to<std::size_t>(parsed_height.at("height"));
            // incoming_connections_count
            auto parsed_incoming = boost::json::parse(response.text);
            std::size_t incoming = value_to<std::size_t>(parsed_incoming.at("incoming_connections_count"));
            // outgoing_connections_count
            auto parsed_outgoing = boost::json::parse(response.text);
            std::size_t outgoing = value_to<std::size_t>(parsed_outgoing.at("outgoing_connections_count"));
            // top_block_hash
            auto parsed_top_block_hash = boost::json::parse(response.text);
            std::string top_block_hash = value_to<std::string>(parsed_top_block_hash.at("top_block_hash"));
            // tx_count
            auto parsed_tx_count = boost::json::parse(response.text);
            std::size_t tx_count = value_to<std::size_t>(parsed_tx_count.at("tx_count"));
            // tx_pool_size
            auto parsed_tx_pool_size = boost::json::parse(response.text);
            std::size_t tx_pool_size = value_to<std::size_t>(parsed_tx_pool_size.at("tx_pool_size"));
            number_of_active_nodes++;
            std::lock_guard<std::mutex> lock(Daemon::daemons_pids_mutex);
            LINFO << "N" << index
                  << ", pid: " << Daemon::daemons_pids.at(index)
                  << ", status: " << status
                  << ", height: " << height
                  << ", in: " << incoming
                  << ", out: " << outgoing
                  << ", tx count: " << tx_count
                  << ", tx pool size: " << tx_pool_size
                  << ", top block hash: " << top_block_hash.substr(0, 4)
                  << ", active nodes: " << number_of_active_nodes;
        }
        size_t number_of_active_wallets = 0;
        for (std::size_t index = 0; index < number_of_nodes; ++index) {
            // std::this_thread::sleep_for(std::chrono::milliseconds((rand() % UPDATE_INTERVAL)  * 10));
            cpr::Url url { std::format("http://{}:{}/json_rpc", WalletRPC::RPC_DEFAULT_IP, WalletRPC::RPC_BASE_PORT + index) };
            cpr::Header header { { "Content-Type", "application/json" } };
            std::string request_json = generate_get_address_request();
            cpr::Response response = cpr::Post(url, header, cpr::Body { request_json }, cpr::Timeout(TIMEOUTMS));
            if (response.status_code != 200) {
                LTRACE << "W" << index << " is unresponsive.";
                continue;
            }
            // Get Json and Parse it
            // address
            auto parsed = boost::json::parse(response.text);
            // check parsed json to see has any error
            if (parsed.as_object().contains("error")) {
                LERROR << "W" << index << " has error: " << parsed.at("error");
                continue;
            }
            std::string address = value_to<std::string>(parsed.at("result").at("address"));
            request_json = generate_get_balance_request();
            response = cpr::Post(url, header, cpr::Body { request_json }, cpr::Timeout(TIMEOUTMS));
            if (response.status_code != 200) {
                LTRACE << "W" << index << " address : " << address << " is unresponsive.";
                continue;
            }
            // balance and unlocked balance
            auto parsed_balance = boost::json::parse(response.text);
            std::size_t balance = value_to<std::size_t>(parsed_balance.at("result").at("balance"));
            std::size_t unlocked_balance = value_to<std::size_t>(parsed_balance.at("result").at("unlocked_balance"));
            number_of_active_wallets++;

            LINFO << "W" << index
                  << " address: " << address.substr(0, 6)
                  << " balance: " << balance
                  << " unlocked balance: " << unlocked_balance
                  << " wallets: " << number_of_active_wallets;
        }
        LINFO << "======================================================================";
    }
}

int main(int argc, char** argv)
{

    // Clean up processes, and kill all monerod or monero-wallet-rpc processes

    parse_and_validate(argc, argv);

    LINFO << "Start daemons...";
    std::atomic<bool> daemon_stop_condition(false);
    std::thread daemon_thread(run_daemons, std::ref(daemon_stop_condition), number_of_nodes, test_duration);

    LINFO << "Start wallets...";
    std::atomic<bool> wallet_stop_condition(false);
    std::thread wallet_thread(run_wallets, std::ref(wallet_stop_condition), number_of_nodes, test_duration);
    // delete_wallet_caches();

    std::atomic<bool> status_reporter_stop_condition(false);
    std::thread status_reporter_thread(status_reporter, std::ref(status_reporter_stop_condition), number_of_nodes);

    // // delete wallet caches
    // delete_wallet_caches();
    // // do benchmark with master
    // LINFO << "Start benchmarking with master...";
    // auto master_duration = do_benchmark(Daemon::MASTER_EXEC_PATH);

    // // delete wallet caches
    // delete_wallet_caches();
    // // do benchmark with test
    // LINFO << "Start benchmarking with test...";
    // auto test_duration = do_benchmark(Daemon::TEST_EXEC_PATH);

    // if((test_duration / 1000).count() != (master_duration / 1000).count()) {
    //     if (test_duration.count() < master_duration.count()) {
    //         LINFO << "PR (test) was faster.";
    //     } else {
    //         LINFO << "master was faster.";
    //     }
    // }

    if (status_reporter_thread.joinable())
        status_reporter_thread.join();
    return 0;
}
