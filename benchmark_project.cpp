
#include <boost/chrono/duration.hpp>
#include <boost/filesystem/directory.hpp>
#include <boost/filesystem/file_status.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/json.hpp>
#include <boost/json/array.hpp>
#include <boost/json/kind.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/serializer.hpp>
#include <boost/json/value.hpp>
#include <boost/log/trivial.hpp>
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
#include <condition_variable>
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
#include <string>
#include <thread>

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

#define MAX_NUMBER_OF_WALLETS (1024 << 2)

namespace WalletRPC {

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
} // namespace WalletRPC

namespace Daemon {
const std::string& EXEC_NAME = "monerod";
filesystem::path EXEC_PATH;

constexpr const int RPC_BASE_PORT = 4096;
constexpr const int P2P_BASE_PORT = 28081;
constexpr const std::string RPC_DEFAULT_IP = "127.0.0.1";

constexpr const char P2P_BIND_IP_CONTAINER[] = "--p2p-bind-ip={}";
constexpr const char P2P_BIND_PORT_CONTAINER[] = "--p2p-bind-port={}";
constexpr const char RPC_BIND_PORT_CONTAINER[] = "--rpc-bind-port={}";
constexpr const char DATA_DIR_CONTAINER[] = "--data-dir={}";
constexpr const char ADD_EXCLUSIVE_NODE_CONTAINER[] = "--add-exclusive-node={}";
constexpr const char DIFFICULTY_CONTAINER[] = "--fixed-difficulty={}";
constexpr const char NO_SYNC[] = "--no-sync";
constexpr const char NO_ZMQ[] = "--no-zmq";
constexpr const char OFFLINE[] = "--offline";
constexpr const char LOG_LEVEL_CONTAINER[] = "--log-level={}";
constexpr const char TESTNET[] = "--testnet";
constexpr const char MAINNET[] = "--mainnet";
constexpr const char NON_INTERACTIVE[] = "--non-interactive";

struct daemon {
    std::string ip_address = RPC_DEFAULT_IP;

    int log_level = 0;
    size_t p2p_port;
    size_t rpc_port;
    std::string p2p_ip;
    std::string data_dir;
    std::vector<std::string> exclusive_nodes;
};
std::string Default_daemon_location()
{
    return test_root + filesystem::path::preferred_separator + "daemons";
}
// Temporary
constexpr const char CONFIG_FILE_CONTAINER[] = "--config-file={}";
} // namespace Daemon

void run_wallet_rpc(WalletRPC::walletrpc& walletrpc, boost::condition_variable& terminator)
{

    process::ipstream pipe_stream;

    process::child wallet_rpc_process(WalletRPC::EXEC_PATH,
        WalletRPC::NO_INITIAL_SYNC,
        WalletRPC::TESTNET,
        WalletRPC::DISABLE_RPC_LOGIN,
        WalletRPC::NON_INTERACTIVE,
        std::format(WalletRPC::DAEMON_ADDRESS_CONTAINER, walletrpc.daemon_ip_address, std::to_string(walletrpc.daemon_port)),
        std::format(WalletRPC::WALLET_DIR_CONTAINER, walletrpc.wallet_dir),
        std::format(WalletRPC::LOG_LEVEL_CONTAINER, walletrpc.log_level),
        std::format(WalletRPC::SHARED_RINGDB_DIR_CONTAINER, walletrpc.shared_ringdb_dir),
        std::format(WalletRPC::MAX_CONCURRENCY_CONTAINER, std::to_string(walletrpc.max_concurrency)),
        std::format(WalletRPC::RPC_BIND_PORT_CONTAINER, std::to_string(walletrpc.rpc_port)),
        process::std_out > pipe_stream);

    std::string line;

    // while (pipe_stream && std::getline(pipe_stream, line) && !line.empty()) {
    //     LTRACE << "WALLET_RPC : " << line;
    // };

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
    if (!filesystem::exists(Daemon::EXEC_PATH)) {
        LDIE(Daemon::EXEC_PATH.string() + " does not exist.", -1);
    }

    process::ipstream pipe_stream;

    process::child daemon_process(Daemon::EXEC_PATH,
        Daemon::NO_SYNC,
        Daemon::OFFLINE,
        Daemon::NON_INTERACTIVE, Daemon::NO_ZMQ,
        std::format(Daemon::P2P_BIND_IP_CONTAINER, daemon.p2p_ip),
        std::format(Daemon::P2P_BIND_PORT_CONTAINER, std::to_string(daemon.p2p_port)),
        std::format(Daemon::RPC_BIND_PORT_CONTAINER, std::to_string(daemon.rpc_port)),
        std::format(Daemon::DATA_DIR_CONTAINER, daemon.data_dir),
        std::format(Daemon::LOG_LEVEL_CONTAINER, std::to_string(daemon.log_level)),
        process::std_out > pipe_stream);

    std::string line;

    // while (pipe_stream && std::getline(pipe_stream, line) && !line.empty()) {
    //     LTRACE << "DAEMON : " << line;
    // };

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

void parse_and_validate(int argc, char** argv)
{
    po::options_description desc("Allowed options");

    desc.add_options()("builddir",
        po::value<std::string>(),
        "A directory that cointains monero executables.");

    desc.add_options()(
        "test_root",
        po::value<std::string>(),
        "A directory that will contain all the blockchains and wallets.");

    desc.add_options()("number_of_wallets",
        po::value<uint32_t>(),
        "Number of wallets that should be generated.");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    std::string builddir;
    if (vm.count("builddir")) {
        builddir = vm["builddir"].as<std::string>();
        if (!builddir.empty() && filesystem::exists(builddir) && filesystem::is_directory(builddir)) {
            // LINFO << "builddir is " << builddir;
            WalletRPC::EXEC_PATH = builddir + filesystem::path::preferred_separator + WalletRPC::EXEC_NAME;
            Daemon::EXEC_PATH = builddir + filesystem::path::preferred_separator + Daemon::EXEC_NAME;
        }
    } else {
        LFATAL << "builddir is not defined.";
        WalletRPC::EXEC_PATH = process::search_path(WalletRPC::EXEC_NAME);
        Daemon::EXEC_PATH = process::search_path(Daemon::EXEC_NAME);
        if (WalletRPC::EXEC_PATH.string().empty()) {
            LDIE("Cannot find monero-wallet-rpc executable.", -1);
        }
        LFATAL << "Will use " << WalletRPC::EXEC_PATH << " as monero-wallet-rpc.";
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

    number_of_wallets = 3;
    if (vm.count("number_of_wallets")) {
        if (vm["number_of_wallets"].as<uint32_t>() < MAX_NUMBER_OF_WALLETS) {
            number_of_wallets = vm["number_of_wallets"].as<uint32_t>();
        } else {
            LDIE("number_of_wallets is invalid.", -1);
        }
    }
    // LINFO << "number_of_wallets is " << number_of_wallets;
}

void create_n_wallets()
{
    Daemon::daemon daemon;
    daemon.ip_address = daemon.p2p_ip = Daemon::RPC_DEFAULT_IP;
    daemon.p2p_port = Daemon::P2P_BASE_PORT;
    daemon.rpc_port = Daemon::RPC_BASE_PORT;
    daemon.data_dir = Daemon::Default_daemon_location();
    daemon.log_level = 0;
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
        walletrpc.max_concurrency = 2;
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

    std::this_thread::sleep_for(std::chrono::seconds(number_of_wallets > 10 ? number_of_wallets : 10));

    auto wallet_creater_call = [&](int index) {
        // LINFO << "RPC call for creating " << index;
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
        // LINFO << "Sending JSON-RPC request : " << boost::json::serialize(rpc_request);
        cpr::Url url { std::format("{}:{}/json_rpc", WalletRPC::RPC_DEFAULT_IP, WalletRPC::RPC_BASE_PORT + index) };
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

        // curl http://127.0.0.1:20057/json_rpc -d '{"jsonrpc":"2.0","id":"0","method":"get_address"}' -H 'Content-Type: application/json'
        // call get address and save the wallet address

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

int main(int argc, char** argv)
{

    parse_and_validate(argc, argv);

    create_n_wallets();

    return 0;
}
