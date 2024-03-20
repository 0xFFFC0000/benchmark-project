
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
#include <cstdint>
#include <cstdio>
#include <exception>
#include <format>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <rpc/client.h>

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
uint32_t number_of_wallets;

// how long it should mine
uint32_t mine_time;

// how many times a cycle (open_wallet / refresh / close) should run.
uint32_t benchmark_iteration;

#define MAX_NUMBER_OF_WALLETS (1024 << 2)
#define MAX_MINE_TIME (1024 << 2)
#define MAX_BENCHMARK_ITERATION_TIME 1024

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
constexpr const char LOG_LEVEL_CONTAINER[] = "--log-level={}";
constexpr const char DAEMON_SSL_ALLOW_ANY_CERT[] = "--daemon-ssl-allow-any-cert";
constexpr const char DISABLE_RPC_LOGIN[] = "--disable-rpc-login";
constexpr const char SHARED_RINGDB_DIR_CONTAINER[] = "--shared-ringdb-dir={}";
constexpr const char NON_INTERACTIVE[] = "--non-interactive";
constexpr const char RPC_SSL_CONTAINER[] = "--rpc-ssl=disabled";
constexpr const char DAEMON_SSL_CONTAINER[] = "--daemon-ssl=disabled";

struct walletrpc {
    std::string ip_address = RPC_DEFAULT_IP;

    size_t rpc_port;
    size_t max_concurrency = 128;
    std::string daemon_ip_address;
    size_t daemon_port;
    std::string wallet_dir;
    std::string password;
    int log_level = 0;
    std::string shared_ringdb_dir;
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
constexpr const char LOG_LEVEL_CONTAINER[] = "--log-level={}";
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
    int log_level = 0;
    size_t max_concurrency = 128;
    size_t max_connections_per_ip = 2048;
    size_t block_sync_size = 2048 << 16;
    size_t p2p_port;
    size_t mining_threads = 2;
    size_t rpc_port;
    size_t difficulty;
    std::string p2p_ip;
    std::string data_dir;
    std::vector<std::string> exclusive_nodes;
};

std::string Default_daemon_location(int index)
{
    return test_root + filesystem::path::preferred_separator + "daemons" + filesystem::path::preferred_separator + std::to_string(index);
}
} // namespace Daemon

void run_wallet_rpc(WalletRPC::walletrpc& walletrpc, boost::condition_variable& terminator)
{
    process::ipstream pipe_stream;
    process::child wallet_rpc_process(WalletRPC::EXEC_PATH,
        WalletRPC::NO_INITIAL_SYNC, WalletRPC::TESTNET, WalletRPC::DISABLE_RPC_LOGIN,
        WalletRPC::NON_INTERACTIVE, WalletRPC::RPC_SSL_CONTAINER, WalletRPC::DAEMON_SSL_CONTAINER,
        std::format(WalletRPC::DAEMON_ADDRESS_CONTAINER, walletrpc.daemon_ip_address, std::to_string(walletrpc.daemon_port)),
        std::format(WalletRPC::WALLET_DIR_CONTAINER, walletrpc.wallet_dir),
        std::format(WalletRPC::LOG_LEVEL_CONTAINER, walletrpc.log_level),
        std::format(WalletRPC::SHARED_RINGDB_DIR_CONTAINER, walletrpc.shared_ringdb_dir),
        std::format(WalletRPC::MAX_CONCURRENCY_CONTAINER, std::to_string(walletrpc.max_concurrency)),
        std::format(WalletRPC::RPC_BIND_PORT_CONTAINER, std::to_string(walletrpc.rpc_port)),
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

void create_offline_daemon(Daemon::daemon& daemon, boost::condition_variable& terminator)
{
    if (!filesystem::exists(daemon.exec_path)) {
        LDIE(daemon.exec_path.string() + " does not exist.", -1);
    }

    process::ipstream pipe_stream;

    process::child daemon_process(daemon.exec_path,
        Daemon::NO_SYNC, Daemon::OFFLINE, Daemon::NON_INTERACTIVE, Daemon::NO_ZMQ,
        Daemon::RPC_SSL_CONTAINER,
        std::format(Daemon::P2P_BIND_IP_CONTAINER, daemon.p2p_ip),
        std::format(Daemon::P2P_BIND_PORT_CONTAINER, std::to_string(daemon.p2p_port)),
        std::format(Daemon::RPC_BIND_PORT_CONTAINER, std::to_string(daemon.rpc_port)),
        std::format(Daemon::MAX_CONCURRENCY_CONTAINER, std::to_string(daemon.max_concurrency)),
        std::format(Daemon::DATA_DIR_CONTAINER, daemon.data_dir),
        std::format(Daemon::LOG_LEVEL_CONTAINER, std::to_string(daemon.log_level)),
        process::std_out > pipe_stream);

    std::string line;

    // wait for signal
    boost::mutex mutex;
    boost::unique_lock<boost::mutex> lock(mutex);
    terminator.wait(lock);

    LTRACE << "Terminating daemon.";
    daemon_process.terminate();
    if (daemon_process.joinable())
        daemon_process.join();
    return;
}

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

    desc.add_options()("number_of_wallets",
        po::value<uint32_t>(),
        "Number of wallets that should be generated.");

    desc.add_options()("mine_time",
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

    test_root = filesystem::path(getenv("HOME")).string() + filesystem::path::preferred_separator + "testnet";
    if (vm.count("test_root")) {
        test_root = vm["test_root"].as<std::string>();
        if (!test_root.empty() && filesystem::exists(test_root) && filesystem::is_directory(test_root)) {
            // LINFO << "test_root is " << test_root;
        } else if (!test_root.empty() && !filesystem::exists(test_root)) {
            filesystem::create_directory(test_root);
            // LINFO << "test_root is " << test_root;
        }
    } else {
        LFATAL << "test_root is not defined.";
        LFATAL << "Will use " << test_root << " as test root directory.";
    }

    number_of_wallets = 12;
    if (vm.count("number_of_wallets")) {
        if (vm["number_of_wallets"].as<uint32_t>() < MAX_NUMBER_OF_WALLETS) {
            number_of_wallets = vm["number_of_wallets"].as<uint32_t>();
        } else {
            LDIE("number_of_wallets is invalid.", -1);
        }
    }

    mine_time = 30;
    if (vm.count("mine_time")) {
        if (vm["mine_time"].as<uint32_t>() < MAX_MINE_TIME) {
            mine_time = vm["mine_time"].as<uint32_t>();

        } else {
            LDIE("mine_time is invalid.", -1);
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

    LINFO << "mine_time is " << mine_time;
    LINFO << "number_of_wallets is " << number_of_wallets;
    LINFO << "benchmark_iteration is " << benchmark_iteration;
}

std::string generate_create_wallet_request(int index)
{
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

std::string generate_check_balance_request()
{
    boost::json::key_value_pair account_index("account_index", 0);
    boost::json::key_value_pair address_indices("address_indices", "[0,0]");
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

std::string generate_refresh_request()
{
    boost::json::key_value_pair start_height("start_height", 0);
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

std::string generate_get_address_request()
{
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

std::string generate_close_wallet_request()
{
    boost::json::key_value_pair jsonrpc("jsonrpc", "2.0");
    boost::json::key_value_pair method("method", "close_wallet");
    boost::json::key_value_pair id("id", "2.0");

    boost::json::object rpc_request;
    rpc_request.insert(jsonrpc);
    rpc_request.insert(id);
    rpc_request.insert(method);
    std::string request_json = boost::json::serialize(rpc_request);
    return request_json;
}

void create_n_wallets()
{
    Daemon::daemon daemon;
    daemon.ip_address = daemon.p2p_ip = Daemon::RPC_DEFAULT_IP;
    daemon.p2p_port = Daemon::P2P_BASE_PORT;
    daemon.rpc_port = Daemon::RPC_BASE_PORT;
    daemon.data_dir = Daemon::Default_daemon_location(1);
    daemon.log_level = 0;
    daemon.exec_path = Daemon::MASTER_EXEC_PATH;
    boost::condition_variable daemon_terminator;
    std::thread daemon_thread(create_offline_daemon, std::ref(daemon), std::ref(daemon_terminator));

    if (!filesystem::exists(WalletRPC::EXEC_PATH)) {
        LDIE(WalletRPC::EXEC_PATH.string() + " does not exist.", -1);
    }

    if (filesystem::exists(WalletRPC::Default_wallet_location())) {
        try {
            filesystem::remove(WalletRPC::Default_wallet_location());
        } catch (boost::filesystem::filesystem_error fe) {
            LDIE(fe.what(), -1);
        }
    }

    filesystem::create_directory(WalletRPC::Default_wallet_location());

    boost::condition_variable wallet_terminator;
    auto wallet_rpc_creator = [&](int index) {
        WalletRPC::walletrpc walletrpc;
        walletrpc.rpc_port = WalletRPC::RPC_BASE_PORT + index;
        walletrpc.password = "''";
        walletrpc.daemon_port = Daemon::RPC_BASE_PORT;
        walletrpc.ip_address = walletrpc.daemon_ip_address = WalletRPC::RPC_DEFAULT_IP;
        walletrpc.log_level = 0;
        walletrpc.wallet_dir = WalletRPC::Default_wallet_location();
        walletrpc.shared_ringdb_dir = WalletRPC::Default_wallet_location();
        std::thread wallet_rpc_thread(run_wallet_rpc, std::ref(walletrpc), std::ref(wallet_terminator));
        if (wallet_rpc_thread.joinable())
            wallet_rpc_thread.join();
    };

    std::vector<std::thread> wallet_rpc_jobs;
    for (int index = 0; index < number_of_wallets; ++index) {
        wallet_rpc_jobs.push_back(std::thread(wallet_rpc_creator, index));
    }

    // std::this_thread::sleep_for(std::chrono::seconds(number_of_wallets > 10 ? number_of_wallets : 10));
    std::this_thread::sleep_for(std::chrono::seconds(10));

    auto wallet_creater_call = [&](int index) {
        std::string request_json = generate_create_wallet_request(index);
        cpr::Url url { std::format("http://{}:{}/json_rpc", WalletRPC::RPC_DEFAULT_IP, WalletRPC::RPC_BASE_PORT + index) };
        cpr::Header header { { "'Content-Type", "application/json" } };

        long status_code = 0;
        do {
            cpr::Response response = cpr::Post(url, header, cpr::Body { request_json });
            status_code = response.status_code;
            if (response.status_code != 200) {
                // LINFO << "monero-wallet-rpc return incorrect  " << response.status_code << " as status_code for wallet : " << std::to_string(index);
                // LINFO << "Because of this failure, try running these benchmarks multiple times.";
            }
        } while (status_code != 200);

        // call get address and save the wallet address
        request_json = generate_get_address_request();
        cpr::Response response = cpr::Post(url, header, cpr::Body { request_json });
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
    for (int index = 0; index < number_of_wallets; ++index) {
        wallet_creator_jobs.push_back(std::thread(wallet_creater_call, index));
    }

    for (int index = 0; index < number_of_wallets; ++index) {
        if (wallet_creator_jobs.at(index).joinable())
            wallet_creator_jobs.at(index).join();
    }

    wallet_terminator.notify_all();
    daemon_terminator.notify_all();

    for (int index = 0; index < number_of_wallets; ++index) {
        if (wallet_rpc_jobs.at(index).joinable())
            wallet_rpc_jobs.at(index).join();
    }

    if (daemon_thread.joinable())
        daemon_thread.join();
}

void run_miner(Daemon::daemon& daemon, int index, int seconds)
{

    process::ipstream pipe_stream;

    std::vector<std::string> exclusive_nodes;

    for (int i = 0; i < number_of_wallets; ++i) {
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
        Daemon::NO_ZMQ, Daemon::TESTNET, Daemon::NO_IGD, Daemon::HIDE_MY_PORT,
        Daemon::DISABLE_RPC_BAN, Daemon::CONFIRM_EXTERNAL_BIND, Daemon::ALLOW_LOCAL_IP,
        Daemon::DISABLE_DNS_CHECKPOINTS, Daemon::RPC_SSL_CONTAINER,
        std::format(Daemon::MAX_CONNECTIONS_PER_IP, daemon.max_connections_per_ip),
        std::format(Daemon::RPC_BIND_IP_CONTAINER, daemon.ip_address),
        std::format(Daemon::BLOCK_SYNC_SIZE_CONTAINER, daemon.block_sync_size),
        std::format(Daemon::MINING_THREADS, daemon.mining_threads),
        std::format(Daemon::P2P_BIND_IP_CONTAINER, daemon.p2p_ip),
        std::format(Daemon::DIFFICULTY_CONTAINER, daemon.difficulty),
        std::format(Daemon::P2P_BIND_PORT_CONTAINER, std::to_string(daemon.p2p_port)),
        std::format(Daemon::RPC_BIND_PORT_CONTAINER, std::to_string(daemon.rpc_port)),
        std::format(Daemon::MAX_CONCURRENCY_CONTAINER, std::to_string(daemon.max_concurrency)),
        std::format(Daemon::DATA_DIR_CONTAINER, daemon.data_dir),
        std::format(Daemon::LOG_LEVEL_CONTAINER, std::to_string(daemon.log_level)),
        std::format(Daemon::START_MINING_CONTAINER, WalletRPC::address_wallet.at(index)),
        exclusive_nodes,
        process::std_out > pipe_stream);

    std::string line;

    // while (pipe_stream && std::getline(pipe_stream, line) && !line.empty()) {
    //     LTRACE << "MINER : " << line;
    // };

    // wait for signal
    std::this_thread::sleep_for(std::chrono::seconds(seconds));

    daemon_process.terminate();
    if (daemon_process.joinable())
        daemon_process.join();
    return;
}

void mine()
{
    auto miner_creator = [&](int index) {
        Daemon::daemon daemon;
        daemon.p2p_port = Daemon::P2P_BASE_PORT + index;
        daemon.rpc_port = Daemon::RPC_BASE_PORT + index;
        daemon.difficulty = 100 + index;
        daemon.ip_address = daemon.p2p_ip = WalletRPC::RPC_DEFAULT_IP;
        daemon.log_level = 0;
        daemon.data_dir = Daemon::Default_daemon_location(index);
        daemon.exec_path = Daemon::MASTER_EXEC_PATH;
        std::thread daemon_thread(run_miner, std::ref(daemon), index, mine_time);
        if (daemon_thread.joinable())
            daemon_thread.join();
    };

    std::vector<std::thread> daemon_miner_jobs;
    for (int index = 0; index < number_of_wallets; ++index) {
        daemon_miner_jobs.push_back(std::thread(miner_creator, index));
    }

    for (int index = 0; index < number_of_wallets; ++index) {
        if (daemon_miner_jobs.at(index).joinable())
            daemon_miner_jobs.at(index).join();
    }
}

void delete_wallet_cache_for_ith(int index)
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
        [&](const std::pair<int, std::string>& item) {
            delete_wallet_cache_for_ith(item.first);
        });
}

std::chrono::milliseconds do_benchmark(filesystem::path daemon_exec_path)
{
    Daemon::daemon daemon;
    daemon.ip_address = daemon.p2p_ip = Daemon::RPC_DEFAULT_IP;
    daemon.p2p_port = Daemon::P2P_BASE_PORT;
    daemon.rpc_port = Daemon::RPC_BASE_PORT;
    daemon.data_dir = Daemon::Default_daemon_location(1);
    daemon.exec_path = daemon_exec_path;
    daemon.log_level = 0;
    boost::condition_variable daemon_terminator;
    std::thread daemon_thread(create_offline_daemon, std::ref(daemon), std::ref(daemon_terminator));

    // if (!filesystem::exists(WalletRPC::EXEC_PATH)) {
    //     LDIE(WalletRPC::EXEC_PATH.string() + " does not exist.", -1);
    // }

    // boost::condition_variable wallet_terminator;
    // auto wallet_rpc_creator = [&](int index) {
    //     WalletRPC::walletrpc walletrpc;
    //     walletrpc.rpc_port = WalletRPC::RPC_BASE_PORT + index;
    //     walletrpc.password = "''";
    //     walletrpc.daemon_port = Daemon::RPC_BASE_PORT;
    //     walletrpc.ip_address = walletrpc.daemon_ip_address = WalletRPC::RPC_DEFAULT_IP;
    //     walletrpc.log_level = 0;
    //     walletrpc.max_concurrency = 128;
    //     walletrpc.wallet_dir = WalletRPC::Default_wallet_location();
    //     walletrpc.shared_ringdb_dir = WalletRPC::Default_wallet_location();
    //     std::thread wallet_rpc_thread(run_wallet_rpc, std::ref(walletrpc), std::ref(wallet_terminator));
    //     if (wallet_rpc_thread.joinable())
    //         wallet_rpc_thread.join();
    // };

    // std::vector<std::thread> wallet_rpc_jobs;
    // for (int index = 0; index < number_of_wallets; ++index) {
    //     wallet_rpc_jobs.push_back(std::thread(wallet_rpc_creator, index));
    // }

    // std::this_thread::sleep_for(std::chrono::seconds(number_of_wallets > 10 ? number_of_wallets : 10));
    std::this_thread::sleep_for(std::chrono::seconds(10));

    auto wallet_benchmark = [&](int max_iter, int index) {
        for (int iter = 0; iter < max_iter; ++iter) {
            // Creating a client that connects to the localhost on port 8080
            rpc::client client(WalletRPC::RPC_DEFAULT_IP, WalletRPC::RPC_BASE_PORT + index);
            // Calling a function with paramters and converting the result to int
            auto result = client.call("get_blocks_by_height.bin", 1).as<char*>();
            for (int i = 0; i < 32; ++i) {
                std::cout << "The result is: " << result[i] << std::endl;
            }

            // std::string request_json = generate_open_wallet_request(index);
            // cpr::Url url { std::format("http://{}:{}/json_rpc", WalletRPC::RPC_DEFAULT_IP, WalletRPC::RPC_BASE_PORT + index) };
            // cpr::Header header { { "'Content-Type", "application/json" } };

            // long status_code = 0;
            // // do {
            // cpr::Response response = cpr::Post(url, header, cpr::Body { request_json });
            // status_code = response.status_code;
            // if (status_code == 200) {
            //     LTRACE << "Wallet " << std::to_string(index) << " opened.";
            // } else if (response.status_code != 200) {
            //     LTRACE << "Wallet incorrect  " << response.status_code << " as status_code for wallet open : " << std::to_string(index);
            // }
            // // } while (status_code != 200);

            // // call get address and save the wallet address
            // request_json = generate_check_balance_request();
            // response = cpr::Post(url, header, cpr::Body { request_json });
            // status_code = response.status_code;
            // if (status_code == 200) {
            //     LTRACE << "Wallet " << std::to_string(index) << " get_balance.";
            // } else {
            //     LTRACE << "Wallet incorrect  " << response.status_code << " as status_code for wallet get_balance : " << std::to_string(index);
            // }

            // // call get address and save the wallet address
            // request_json = generate_refresh_request();
            // response = cpr::Post(url, header, cpr::Body { request_json });
            // status_code = response.status_code;
            // if (status_code == 200) {
            //     LTRACE << "Wallet " << std::to_string(index) << " refreshed.";
            // } else {
            //     LTRACE << "Wallet incorrect  " << response.status_code << " as status_code for wallet refresh : " << std::to_string(index);
            // }

            // // call get address and save the wallet address
            // request_json = generate_check_balance_request();
            // response = cpr::Post(url, header, cpr::Body { request_json });
            // status_code = response.status_code;
            // if (status_code == 200) {
            //     LTRACE << "Wallet " << std::to_string(index) << " get_balance.";
            // } else {
            //     LTRACE << "Wallet incorrect  " << response.status_code << " as status_code for wallet get_balance : " << std::to_string(index);
            // }

            // // call get address and save the wallet address
            // request_json = generate_close_wallet_request();
            // response = cpr::Post(url, header, cpr::Body { request_json });
            // status_code = response.status_code;
            // if (status_code == 200) {
            //     LTRACE << "Wallet " << std::to_string(index) << " closed.";
            // } else {
            //     LTRACE << "Wallet incorrect  " << response.status_code << " as status_code for wallet close : " << std::to_string(index);
            // }

            // delete_wallet_cache_for_ith(index);
        }
    };

    std::vector<std::thread> warm_up_jobs;
    warm_up_jobs.clear();
    for (int index = 0; index < number_of_wallets; ++index) {
        warm_up_jobs.push_back(std::thread(wallet_benchmark, 1, index));
    }

    for (int index = 0; index < number_of_wallets; ++index) {
        if (warm_up_jobs.at(index).joinable())
            warm_up_jobs.at(index).join();
    }

    std::this_thread::sleep_for(std::chrono::seconds(10));

    auto start = std::chrono::steady_clock::now();
    LINFO << "Run benchmarks...";

    std::vector<std::thread> wallet_creator_jobs;
    wallet_creator_jobs.clear();
    for (int index = 0; index < number_of_wallets; ++index) {
        wallet_creator_jobs.push_back(std::thread(wallet_benchmark, benchmark_iteration, index));
    }

    for (int index = 0; index < number_of_wallets; ++index) {
        if (wallet_creator_jobs.at(index).joinable())
            wallet_creator_jobs.at(index).join();
    }

    // wallet_terminator.notify_all();
    daemon_terminator.notify_all();

    // for (int index = 0; index < number_of_wallets; ++index) {
    //     if (wallet_rpc_jobs.at(index).joinable())
    //         wallet_rpc_jobs.at(index).join();
    // }

    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    int benchmark_seconds = (elapsed / 1000).count();
    int benchmark_miliseconds = (elapsed % 1000).count();
    LINFO
        << ">>> Total elapsed time for running the benchmark: " << benchmark_seconds << "." << benchmark_miliseconds;

    if (daemon_thread.joinable())
        daemon_thread.join();
    return elapsed;
}

int main(int argc, char** argv)
{
    parse_and_validate(argc, argv);

    create_n_wallets();
    delete_wallet_caches();

    std::for_each(begin(WalletRPC::address_wallet), end(WalletRPC::address_wallet),
        [&](const std::pair<int, std::string>& item) {
            LTRACE << "Wallet " << item.first << " : " << item.second;
        });

    LINFO << "Start mining...";
    mine();

    // delete wallet caches
    delete_wallet_caches();
    // do benchmark with master
    LINFO << "Start benchmarking with master...";
    auto master_duration = do_benchmark(Daemon::MASTER_EXEC_PATH);

    // delete wallet caches
    delete_wallet_caches();
    // do benchmark with test
    LINFO << "Start benchmarking with test...";
    auto test_duration = do_benchmark(Daemon::TEST_EXEC_PATH);

    // if((test_duration / 1000).count() != (master_duration / 1000).count()) {
    //     if (test_duration.count() < master_duration.count()) {
    //         LINFO << "PR (test) was faster.";
    //     } else {
    //         LINFO << "master was faster.";
    //     }
    // }

    return 0;
}
