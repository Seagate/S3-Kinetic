#include "p2p_operation_executor.h"
#include "p2p_put_callback.h"
#include "p2p_push_callback.h"
#include <memory>
#include <sstream>
#include <vector>
#include <stddef.h>

namespace com {
namespace seagate {
namespace kinetic {

using std::string;
using std::stringstream;
using std::shared_ptr;
using std::unique_ptr;
using std::vector;
using std::make_shared;
using std::chrono::high_resolution_clock;
using std::chrono::microseconds;
using std::chrono::duration_cast;

using ::kinetic::NonblockingKineticConnection;
using ::kinetic::KineticConnectionFactory;
using ::kinetic::KineticStatus;
using ::kinetic::P2PPushRequest;
using ::kinetic::P2PPushOperation;

using proto::Command_Status_StatusCode;
using proto::Command_Status_StatusCode_IsValid;
using proto::Command_Status_StatusCode_SUCCESS;
using proto::Command_Status_StatusCode_INTERNAL_ERROR;
using proto::Command_Status_StatusCode_REMOTE_CONNECTION_ERROR;
using proto::Command_Status_StatusCode_NOT_ATTEMPTED;
using proto::Command_Status_StatusCode_NESTED_OPERATION_ERRORS;
using proto::Command_Status_StatusCode_EXPIRED;
using proto::Command_Status_StatusCode_SERVICE_BUSY;
using proto::Command_Status_StatusCode_NOT_AUTHORIZED;
using proto::Command_P2POperation_Peer;

/////////////////////////////////////////////////////////
/// Used by P2P Request Manager spawned Thread
/// "p2p_operation_executor_thread_"
/// De-queues and operates upon P2PRequests created in p2p_request_manager.
/// Establishes Connection Between Sender and Receiver mapping
///
/// @param[in] request_queue -  ThreadsafeBlockingQueue containing P2PRequest shared pointer objects
/// @param[in] kinetic_connection_factory  -CPP clients factory to create non blocking connections

P2POperationExecutor::P2POperationExecutor(AuthorizerInterface &authorizer,
        UserStoreInterface &user_store,
        KineticConnectionFactory &kinetic_connection_factory,
        SkinnyWaistInterface &skinny_waist,
        size_t heuristic_memory_max_usage,
        size_t heuristic_memory_continue_threshold,
        ThreadsafeBlockingQueue<shared_ptr<P2PRequest>>* request_queue) :
            authorizer_(authorizer),
            user_store_(user_store),
            kinetic_connection_factory_(kinetic_connection_factory),
            skinny_waist_(skinny_waist),
            heuristic_memory_max_usage_(heuristic_memory_max_usage),
            heuristic_memory_continue_threshold_(heuristic_memory_continue_threshold),
            request_queue_(request_queue) {}

/////////////////////////////////////////////////////////
/// P2P Request Manager spawns a Thread "p2p_operation_executor_thread_"
/// tied to this ProcessRequestQueue()function
///
/// The thread dequeues a shared @P2PRequest class pointer from
/// the @ThreadsafeBlockingQueue and assigns it to shared pointer
/// @request_ptr
///
/// Using the member variables of the P2PRequest class,
/// Execute() is called w/ request_ptr's member variables as params
/// --------------------------------
/// -- P2P Push Command Structure --
/// --------------------------------
/// Command{
///    header {}
///    body {
///      p2pOperation {
///        peer{ **Address of Device "B"** }
///
///        operation{ **Repeatable field. For keys (startkey -> endkey-1)**
///          key:
///        }
///
///        operation {  **Optionally If Piped Push, Last Op contains nested p2pOp**
///          key: **endkey**
///          p2pOperation {
///            peer{ **Address of Tertiary device C,(used by "B")** }
///
///            operation{  **Repeatable field**
///              key:
///            }
///          } **end nested p2pop
///        } **end last Operation
///      } **end body
///  } **end Command
/////////////////////////////////////////////////////
void P2POperationExecutor::ProcessRequestQueue() {
    while (true) {
        shared_ptr<P2PRequest> request_ptr = nullptr;
        if (!request_queue_->BlockingRemove(request_ptr)) {
            // False means an interrupt occurred. Stop.
            VLOG(2) << "Request queue received interrupt, stopping worker thread";
            return;
        }

        // Only do work if the request_ptr says that processing can start
        // otherwise ignore this request
        if (request_ptr->StartProcessingIfAllowed()) {
            Execute(request_ptr->GetUserId(),
                    request_ptr->GetDeadline(),
                    request_ptr->GetP2POp(),
                    request_ptr->GetResponse(),
                    *(request_ptr->GetRequestContext()));
            request_ptr->MarkCompletedAndNotify();
        }
    }
}

/////////////////////////////////////////////////////////
/// Carries out all P2P puts, pushes and initiates construction
/// of nested Push Operations.
///
/// Purpose::
/// Initiate construction of nonblocking connection
/// between Sender & Receiver Mappings
///
/// Increment Operation Counters (decrements are done via callbacks)
///
/// Construct Callbacks for Put and Push
///
/// Maintain I.O. and set status of overall P2P push operation
///
/// @param[in] user_id
/// @param[in] deadline    -calculated time indicating permitted execution window
/// @param[in] p2pop   -reference to original P2P Operation located in command body
/// @param[in] response_command    -reference to reponse cmd for status purposes
/// @param[in] request_context

void P2POperationExecutor::Execute(int64_t user_id,
    high_resolution_clock::time_point deadline,
    const Command_P2POperation& p2pop,
    Command* response_command,
    RequestContext& request_context) {
    if (!authorizer_.AuthorizeGlobal(user_id, Domain::kP2Pop, request_context)) {
        SetOverallStatus(response_command,
                Command_Status_StatusCode_NOT_AUTHORIZED,
                "Not authorized for P2P");
        return;
    }

    Command_P2POperation_Peer const &peer = p2pop.peer();
    VLOG(2) << "Sending P2P Push to " << peer.hostname() << ":" << peer.port() << " tls=" << peer.tls();//NO_SPELL

    User user;
    if (!user_store_.Get(user_id, &user)) {
        VLOG(2) << "Unable to load user to execute P2P on behalf of";//NO_SPELL
        LOG(ERROR) << "IE P2P";//NO_SPELL
        SetOverallStatus(response_command,
                proto::Command_Status_StatusCode_INTERNAL_ERROR,
                "Unable to load user to execute P2P on behalf of");
        return;
    }

    //assembly of the connection details to the Peer is constructed
    //and stored into a ConnectionOptions Object
    //Used to construct Sender,Receiver,Service & Connection in CPP client
    ::kinetic::ConnectionOptions options;
    options.host = peer.hostname();
    options.port = peer.port();
    options.use_ssl = peer.tls();
    options.user_id = user_id;
    options.hmac_key = user.key();

#ifdef KDEBUG
    DLOG(INFO) << "P2POperationExecutor::CREATE New Kinetic Connection To:" << peer.hostname();//NO_SPELL//NOLINT
#endif
    // Set up a unique pointer to a new non blocking connection between Sender & Receiver Mappings.
    // Attempt to establish a connection between the two.
    // Hooks up to Connection Factory and creates the following in order (all in the CPP client):
    //
    // 1. @<shared_pointer> socket_wrapper(@options.host, .port, .use_ssl)
    // 2. @<shared_pointer> NonblockingReceiver(@socket_wrapper, @hmac_provider_, @options)
    // 3. @<unique_ptr> NonblockingSender(@socket_wrapper, @receiver, @writer_factory, @options)
    // 4. @NonblockingPacketService* (@socket_wrapper, @sender, @receiver)
    unique_ptr< ::kinetic::NonblockingKineticConnection> nonblocking_connection;
    if (kinetic_connection_factory_.NewNonblockingConnection(
            options, nonblocking_connection).notOk()) {
        VLOG(2) << "P2P unable to connect to " << peer.hostname() << ":" << peer.port() << " for " << user_id;//NO_SPELL
        SetOverallStatus(response_command,
                proto::Command_Status_StatusCode_REMOTE_CONNECTION_ERROR,
                "Unable to connect to peer.");
        return;
    }

    ////////////////////////////////////////////////
    //Counters
    // @attempted_operation_count: incremented for every PUT (every key in original op)
    // && for every issuance of a nested P2P push
    //
    // @successful_operation_count: incremented by each operation’s “callback” handler
    // (called by CPP client Receiver)
    //
    // @attempted_operation_count and @successful_operation_count are compared @
    // completion to initialize bool @all_operations_succeeded
    //
    // @outstanding_puts && @outstanding_pushes: incremented for each Put / Push issuance
    // decremented by callback handler
    int outstanding_puts = 0;
    int outstanding_pushes = 0;
    int successful_operation_count = 0;
    int attempted_operation_count = 0;
    size_t heuristic_cache_size = 0;

    // Add Operation && Initialize all sub-statuses
    // Set to not-attempted so an unrecoverable error
    // during processing will leave these in the correct state.
    // They will be set to REMOTE_CONNECTION_ERROR when attempted, and
    // set to the proper status upon successful or unsuccessful completion.
    for (int i = 0; i < p2pop.operation_size(); i++) {
        response_command->mutable_body()->mutable_p2poperation()->
                add_operation()->mutable_status()->
                set_code(Command_Status_StatusCode_NOT_ATTEMPTED);
    }

    ///////////////////////////////////////////////////////////////////////////////////
    ///W O R K  L O O P (Basic overview)
    //For each operation:
    // create @&operation: new const reference to the original operation
    // create @*result_p2pop: P2POperation Command pointer to corresponding **Response Cmd op
    // create @*result_status: Command_ status pointer mutable status of @*result_p2pop
    //
    // -call skinny waist Get for key stored in the operation for this iteration
    // -Construct Put() along with shared P2P put callback, send to Nonblocking Connection
    // -Check for nested p2pOp (push), if present,
    //     --create @*result_nested_op: Command_P2POperation ptr to result_p2pop->mutable_p2pop()
    //     --Build P2P push request via BuildP2PPushRequest()
    //     --Construct P2P push callback, send to Nonblocking Connection
    //
    // -Instruct Connection to run. (this tells sender to send and receiver to receive)
    // -Repeat until finished
    // -Evaluate counters and handle status
    for (int i = 0; i < p2pop.operation_size(); i++) {
        Command_P2POperation_Operation const &operation = p2pop.operation(i);

        proto::Command_P2POperation_Operation* result_p2pop = response_command->
                mutable_body()->
                mutable_p2poperation()->
                mutable_operation(i);

        proto::Command_Status *result_status = result_p2pop->mutable_status();

        PrimaryStoreValue primary_store_value;
        NullableOutgoingValue outgoing_value;

        switch (skinny_waist_.Get(
                user_id,
                operation.key(),
                &primary_store_value,
                request_context,
                &outgoing_value)) {
            case StoreOperationStatus_SUCCESS:
            {
                VLOG(1) << "StoreOperationStatus_SUCCESS";//NO_SPELL
                string value_string;
                int err;
                if (!outgoing_value.ToString(&value_string, &err)) {
                    if (err == EIO) {
                        LOG(ERROR) << "A Read Error Occurred";
                        skinny_waist_.SetRecordStatus(operation.key());
                        result_status->set_code(proto::Command_Status_StatusCode_PERM_DATA_ERROR);
                    } else {
                        LOG(ERROR) << "IE P2P";//NO_SPELL
                        result_status->set_code(proto::Command_Status_StatusCode_INTERNAL_ERROR);
                    }
                } else {
                    const string& target_key =
                            operation.has_newkey() ? operation.newkey() : operation.key();
                    ::kinetic::WriteMode write_mode =
                            operation.force() ? ::kinetic::WriteMode::IGNORE_VERSION :
                                    ::kinetic::WriteMode::REQUIRE_SAME_VERSION;

                    if (::com::seagate::kinetic::client::proto::Command_Algorithm_IsValid(
                            primary_store_value.algorithm)) {
                        auto record = make_shared<const ::kinetic::KineticRecord>(
                                value_string,
                                primary_store_value.version,
                                primary_store_value.tag,
                                (::com::seagate::kinetic::client::proto::Command_Algorithm)
                                        primary_store_value.algorithm);

                        // The success and failure callbacks will set the statsus appropriately.
                        // Setting it here covers the case where neither callback is called,
                        // which could be a communication error or a timeout.
                        result_status->set_code(Command_Status_StatusCode_REMOTE_CONNECTION_ERROR);
                        result_status->set_statusmessage(
                                "P2P did not complete. May have timed out.");

                        outstanding_puts++;
                        size_t heuristic_put_operation_size = HeuristicPutOperationSize(
                                target_key,
                                record,
                                operation.version());

                        heuristic_cache_size += heuristic_put_operation_size;

                        nonblocking_connection->Put(
                                target_key,
                                operation.version(),
                                write_mode,
                                record,
                                make_shared<P2PPutCallback>(&outstanding_puts,
                                        result_status,
                                        &successful_operation_count,
                                        &heuristic_cache_size,
                                        heuristic_put_operation_size));
                        attempted_operation_count++;
                        //If this put Operation has a nested P2P operation,
                        //Contruct a new P2P push and send to Connection for Processing
                        if (operation.has_p2pop()) {
#ifdef KDEBUG
                            DLOG(INFO) << "P2POpExec::Op on iteration " << i << " has Nested P2Pop";//NO_SPELL//NOLINT
#endif
                            size_t heuristic_p2p_operation_size =
                                    HeuristicP2POperationSize(operation.p2pop());
                            heuristic_cache_size += heuristic_p2p_operation_size;

                            Command_P2POperation* result_nested_op = result_p2pop->mutable_p2pop();
                            P2PPushRequest nested_request = BuildP2PPushRequest(operation.p2pop());

                            outstanding_pushes++;
                            nonblocking_connection->P2PPush(
                                    nested_request,
                                    make_shared<P2PPushCallback>(&outstanding_pushes,
                                            result_status,
                                            &successful_operation_count,
                                            &heuristic_cache_size,
                                            heuristic_p2p_operation_size,
                                            result_nested_op));
                            attempted_operation_count++;
                        }

                        // Call run to try to keep up with IO
                        if (!DoRun(*nonblocking_connection, response_command)) {
                            return;
                        }

                        if (heuristic_cache_size >= heuristic_memory_max_usage_) {
                            VLOG(2) << "Running IO loop, heuristic cache size: <"
                                    << heuristic_cache_size
                                    << "> on operation <" << i << "> of <"
                                    << p2pop.operation_size() << ">.";

                            if (!DoIO(&outstanding_puts, &outstanding_pushes, response_command,
                                deadline, *nonblocking_connection, true, &heuristic_cache_size)) {
                                return;
                            }
                        }
                    } else {
                        LOG(ERROR) << "IE P2P";//NO_SPELL
                        result_status->set_code(proto::Command_Status_StatusCode_INTERNAL_ERROR);
                        result_status->set_statusmessage("Invalid tag algorithm");
                    }
                }
            }
                break;
            case StoreOperationStatus_NOT_FOUND:
                result_status->set_code(proto::Command_Status_StatusCode_NOT_FOUND);
                break;
            case StoreOperationStatus_DATA_CORRUPT:
                attempted_operation_count++;
                result_status->set_code(proto::Command_Status_StatusCode_PERM_DATA_ERROR);
                result_status->set_statusmessage("KEY FOUND, BUT COULD NOT READ");
                break;
            default:
                LOG(ERROR) << "IE P2P";//NO_SPELL
                result_status->set_code(proto::Command_Status_StatusCode_INTERNAL_ERROR);
                break;
        }
    }

    if (!DoIO(&outstanding_puts, &outstanding_pushes, response_command, deadline,
            *nonblocking_connection, false, &heuristic_cache_size)) {
        return;
    }

    if (outstanding_puts > 0 || outstanding_pushes > 0) {
        VLOG(2) << "P2P timed out";//NO_SPELL
        SetOverallStatus(response_command,
                proto::Command_Status_StatusCode_EXPIRED,
                "At least one operation timed out");
    } else {
        // If there are no outstanding puts or pushes, and our successful
        // count matches our attempted count, then all ops were successful
        bool all_operations_succeeded = successful_operation_count == attempted_operation_count;
#ifdef KDEBUG
        DLOG(INFO) << "P2POpExec::op_size: " << p2pop.operation_size();//NO_SPELL
        DLOG(INFO) << "P2POpExec::success_op_ctr: " << successful_operation_count;//NO_SPELL
        DLOG(INFO) << "op_size-success_op_ct " << p2pop.operation_size()-successful_operation_count;//NO_SPELL//NOLINT
#endif
        if (all_operations_succeeded) {
            SetOverallStatus(response_command, Command_Status_StatusCode_SUCCESS, nullptr);
        } else {
            stringstream message;
            message << p2pop.operation_size() - successful_operation_count
                    << " operations were unsuccessful.";
            SetOverallStatus(response_command,
                    Command_Status_StatusCode_NESTED_OPERATION_ERRORS,
                    message.str().c_str());
        }
    }
}

bool P2POperationExecutor::DoIO(int* outstanding_puts,
        int* outstanding_pushes,
        Command* response_command,
        high_resolution_clock::time_point deadline,
        ::kinetic::NonblockingKineticConnection& nonblocking_connection,
        bool abort_when_cache_is_below_threshold,
        size_t* heuristic_cache_size) {
    fd_set read_fds, write_fds;
    int num_fds = 0;

    if (!DoRun(nonblocking_connection, response_command, &read_fds, &write_fds, &num_fds)) {
        return false;
    }

    //Ammended November 2014 by: James DeVore
    //to include check for outstanding pushes.
    while (*outstanding_puts > 0 || *outstanding_pushes > 0) {
        // If we're clearing the cache, and we've reduced usage to below the restart threshold,
        // then we can abort this IO loop and go back to enqueuing P2P Push commands
        if (abort_when_cache_is_below_threshold
                && (*heuristic_cache_size <= heuristic_memory_continue_threshold_)) {
            VLOG(2) << "Aborting IO loop to continue enqueuing requests. "//NO_SPELL
                    << "Heuristic cache size: <" << *heuristic_cache_size << ">.";

            return true;
        }

        // Calculate how many microseconds we have until the deadline
        microseconds usec_remaining = duration_cast<microseconds>(
                deadline - high_resolution_clock::now());

        // If the time remaining is negative, abort
        if (usec_remaining < microseconds(0)) {
            SetOverallStatus(response_command, Command_Status_StatusCode_EXPIRED, "At least one operation timed out");
            return false;
        }

        struct timeval select_timeout;
        select_timeout.tv_sec =  usec_remaining.count() / 1000000;
        select_timeout.tv_usec = usec_remaining.count() % 1000000;

        int number_ready_fds = select(num_fds + 1, &read_fds, &write_fds, NULL, &select_timeout);
        if (number_ready_fds < 0) {
            // select() returned an error
            SetOverallStatus(response_command,
                    Command_Status_StatusCode_REMOTE_CONNECTION_ERROR,
                    "At least one operation encountered an error");
            VLOG(2) << "Select encountered error";

            // A select error is not recoverable, abort.
            return false;
        } else if (number_ready_fds == 0) {
            // select() returned before any sockets were ready meaning the connection timed out
            SetOverallStatus(response_command,
                    Command_Status_StatusCode_EXPIRED,
                    "At least one operation timed out");
            VLOG(2) << "Select timed out";
            // A select error is not recoverable, abort.
            return false;
        }

        if (!DoRun(nonblocking_connection, response_command, &read_fds, &write_fds, &num_fds)) {
            return false;
        }
    }
    return true;
}

bool P2POperationExecutor::DoRun(NonblockingKineticConnection& nonblocking_connection,
        Command* response_command) {
    fd_set read_fds, write_fds;
    int num_fds = 0;
    return DoRun(nonblocking_connection, response_command, &read_fds, &write_fds, &num_fds);
}

bool P2POperationExecutor::DoRun(NonblockingKineticConnection& nonblocking_connection,
        Command* response_command, fd_set* read_fds, fd_set* write_fds, int* num_fds) {
    if (!nonblocking_connection.Run(read_fds, write_fds, num_fds)) {
        SetOverallStatus(response_command,
                Command_Status_StatusCode_REMOTE_CONNECTION_ERROR,
                "At least one operation encountered an error");
        VLOG(2) << "Run did not succeed";
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////
/// SetOverallStatus for a P2P Push request in order to
/// Return to Client or Previous Device in the Pipeline
void P2POperationExecutor::SetOverallStatus(Command* response_command,
                                            Command_Status_StatusCode code,
                                            char const* msg) {
    proto::Command_Status* status = response_command->mutable_status();
    status->set_code(code);
    if (msg != nullptr) {
        status->set_statusmessage(msg);
    }
    // This information seems redundant here, but it will help find errors
    // when nested P2Ps are unsuccessful (when the nesting is only 1-deep,
    // it is equivalent to code == SUCCESS)
    response_command->mutable_body()
            ->mutable_p2poperation()
            ->set_allchildoperationssucceeded(code == Command_Status_StatusCode_SUCCESS);
}

size_t P2POperationExecutor::HeuristicPutOperationSize(
        const string& target_key,
        const std::shared_ptr<const ::kinetic::KineticRecord> record,
        const string& version) {
    size_t size = 0;

    size += sizeof(target_key) + target_key.capacity();
    size += sizeof(*(record->value())) + record->value()->capacity();
    size += sizeof(*(record->version())) + record->version()->capacity();
    size += sizeof(*(record->tag())) + record->tag()->capacity();
    size += sizeof(version) + version.capacity();
    return size;
}

size_t P2POperationExecutor::HeuristicP2POperationSize(Command_P2POperation p2pop) {
    size_t size = 0;
    for (int i = 0; i < p2pop.operation_size(); ++i) {
        Command_P2POperation_Operation const& operation = p2pop.operation(i);
        string key = operation.key();
        size += sizeof(key) + key.capacity();

        if (operation.has_newkey()) {
            string new_key = operation.newkey();
            size += sizeof(new_key) + new_key.capacity();
        }

        if (operation.has_version()) {
            string version = operation.version();
            size += sizeof(version) + version.capacity();
        }
        if (operation.has_p2pop()) {
            size += HeuristicP2POperationSize(operation.p2pop());
        }
    }
    return size;
}

/////////////////////////////////////////////////////////
/// Build new P2PPushRequest based off of the original command's
/// Operation nested p2p operation
///
/// @param[in] p2p_op  -pointer to original command put operation's nested op
///
/// @param[out] nested_request  -P2PPushRequest containing necessary components instructing
///                              next device in pipeline what keys to put to tertiary drive
P2PPushRequest P2POperationExecutor::BuildP2PPushRequest(Command_P2POperation p2p_op) {
    P2PPushRequest nested_request;

    nested_request.host = p2p_op.peer().hostname();
    nested_request.port = p2p_op.peer().port();
    for (int i = 0; i < p2p_op.operation_size(); ++i) {
        auto op = p2p_op.operation(i);
        P2PPushOperation push_op;
        push_op.key = op.key();
        push_op.version = op.version();
        push_op.newKey = op.newkey();
        push_op.force = op.force();

        if (op.has_p2pop()) {
            push_op.request = make_shared<P2PPushRequest>(BuildP2PPushRequest(op.p2pop()));
        }

        nested_request.operations.push_back(push_op);
    }

    return nested_request;
}
} // namespace kinetic
} // namespace seagate
} // namespace com
