#include "setup_handler.h"

#include <sstream>
#include <vector>

#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "command_line_flags.h"
#include "glog/logging.h"
#include "openssl/sha.h"
#include "popen_wrapper.h"
#include "server.h"

namespace com {
namespace seagate {
namespace kinetic {

using proto::Command_Status_StatusCode_SUCCESS;
using proto::Command_Status_StatusCode_NOT_AUTHORIZED;
using proto::Command_Status_StatusCode_INTERNAL_ERROR;

const char SetupHandler::kFIRMWARE_UPDATE_TEMPLATE[] = "/kinetic-firmware-update.XXXXXX";
const char SetupHandler::kFIRMWARE_RUN_TEMPLATE[] = "/kinetic-firmware-run.XXXXXX";

SetupHandler::SetupHandler(
        AuthorizerInterface& authorizer,
        SkinnyWaistInterface& skinny_waist,
        ClusterVersionStoreInterface& cluster_version_store,
        const std::string& firmware_update_tmp_dir,
        SecurityHandlerInterface& security_handler,
        DeviceInformationInterface& device_information)
            : authorizer_(authorizer),
            skinny_waist_(skinny_waist),
            cluster_version_store_(cluster_version_store),
            firmware_update_tmp_dir_(firmware_update_tmp_dir),
            security_handler_(security_handler),
            device_information_(device_information) {
}

void SetupHandler::ProcessRequest(const proto::Command &command,
                                  IncomingValueInterface* request_value,
                                  proto::Command *command_response,
                                  RequestContext& request_context,
                                  uint64_t userid,
                                  bool corrupt) {
    VLOG(3) << "Received command SETUP";

    StoreOperationStatus status = StoreOperationStatus_INTERNAL_ERROR;

    // Setup required for all actions
    if (!authorizer_.AuthorizeGlobal(userid, Domain::kSetup, request_context)) {
        status = StoreOperationStatus_AUTHORIZATION_FAILURE;
    } else if (command.body().setup().setupoptype() == Command_Setup_SetupOpType_CLUSTER_VERSION_SETUPOP) {
        status = HandleSetClusterVersion(command);
    } else if (command.body().setup().setupoptype() == Command_Setup_SetupOpType_FIRMWARE_SETUPOP) {
        status = HandleFirmwareDownload(command, request_value, command_response, corrupt);
    } else {
        // Early exit for invalid request
        LOG(ERROR) << "Invalid Request for Setup Command";
        command_response->mutable_status()->set_code(Command_Status_StatusCode_INVALID_REQUEST);
        if (command.body().setup().setupoptype() == Command_Setup_SetupOpType_INVALID_SETUPOP) {
            command_response->mutable_status()->set_statusmessage("INVALID_SETUPOP please chose a valid setupOpType");
        } else {
            command_response->mutable_status()->set_statusmessage("unknown setupOpType");
        }
        return;
    }

    switch (status) {
        case StoreOperationStatus_SUCCESS:
            command_response->mutable_status()->set_code(Command_Status_StatusCode_SUCCESS);
            break;
        case StoreOperationStatus_AUTHORIZATION_FAILURE:
            command_response->mutable_status()->set_code(Command_Status_StatusCode_NOT_AUTHORIZED);
            command_response->mutable_status()->set_statusmessage("permission denied");
            break;
        case StoreOperationStatus_BLOCK_POINT_MISMATCH:
            command_response->mutable_status()->set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            command_response->mutable_status()->
                set_statusmessage("Format block point. Need to ISE drive before you can update firmware");
            break;
        case StoreOperationStatus_FIRMWARE_INVALID:
            command_response->mutable_status()->set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            command_response->mutable_status()->set_statusmessage("Cannot update firmware. Firmware is not valid");
            break;
        case StoreOperationStatus_UNSUPPORTABLE:
            break;
        case StoreOperationSTatus_PRECHECK_FAILED:
            command_response->mutable_status()->set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            command_response->mutable_status()->set_statusmessage("Cannot update firmware. Precheck failed");
            break;
        default:
            LOG(ERROR) << "IE ProcessRequest";//NO_SPELL
            command_response->mutable_status()->set_code(Command_Status_StatusCode_INTERNAL_ERROR);
            command_response->mutable_status()->set_statusmessage("Unexpected Error in Setup Command");
            break;
    }
}

StoreOperationStatus SetupHandler::HandleSetClusterVersion(const proto::Command &command) {
    int64_t new_cluster_version = command.body().setup().newclusterversion();
    VLOG(1) << "Changing cluster version to " << new_cluster_version;

    if (cluster_version_store_.SetClusterVersion(new_cluster_version)) {
        return StoreOperationStatus_SUCCESS;
    } else {
        LOG(ERROR) << "IE ClusterV";//NO_SPELL
        return StoreOperationStatus_INTERNAL_ERROR;
    }
}

StoreOperationStatus SetupHandler::HandleFirmwareDownload(const proto::Command &command,
        IncomingValueInterface* request_value, proto::Command* command_response, bool corrupt) {
    VLOG(1) << "Received firmware update";

    StoreOperationStatus status = StoreOperationStatus_INTERNAL_ERROR;

    std::string tmp_format(firmware_update_tmp_dir_);
    tmp_format.append(kFIRMWARE_UPDATE_TEMPLATE);

    // Check if firmware file is empty
    if (request_value->size() <= 0) {
        LOG(ERROR) << "Firmware download file is empty";
        command_response->mutable_status()->set_code(Command_Status_StatusCode::Command_Status_StatusCode_INVALID_REQUEST);
        command_response->mutable_status()->set_statusmessage("Firmware download file is empty");
        return StoreOperationStatus_FIRMWARE_INVALID;
    }

    // Attempt to make firmware update directory in the event that it doesn't exist
    if (mkdir(firmware_update_tmp_dir_.c_str(), 0777) != 0 && errno != EEXIST) {
        PLOG(ERROR) << "Failed to create directory";
        command_response->mutable_status()->set_code(Command_Status_StatusCode::Command_Status_StatusCode_INTERNAL_ERROR);
        command_response->mutable_status()->set_statusmessage("Failed to create directory for firmware download");
        return StoreOperationStatus_INTERNAL_ERROR;
    }

    RemoveOldFirmwareFiles(firmware_update_tmp_dir_);
    int tmp_fd = mkstemp(&tmp_format[0]);
    if (tmp_fd == -1) {
        PLOG(ERROR) << "Failed to create a temporary file for firmware update";
       command_response->mutable_status()->set_code(Command_Status_StatusCode::Command_Status_StatusCode_INTERNAL_ERROR);
       command_response->mutable_status()->set_statusmessage("Failed to create a temporary file for firmware update");
    } else if (!request_value->TransferToFile(tmp_fd)) {
        if (close(tmp_fd) == 0) {
           unlink(tmp_format.c_str());
        }
        PLOG(ERROR) << "Could not write firmware update to file.  Utility disk might be full.";
        command_response->mutable_status()->set_code(Command_Status_StatusCode::Command_Status_StatusCode_INTERNAL_ERROR);
        command_response->mutable_status()->set_statusmessage("Could not write firmware update to file.  Utility disk might be full.");
    } else if (close(tmp_fd) != 0) {
        PLOG(ERROR) << "Failed to close temporary file for firmware update";
        command_response->mutable_status()->set_code(Command_Status_StatusCode::Command_Status_StatusCode_INTERNAL_ERROR);
        command_response->mutable_status()->set_statusmessage("Failed to close temporary file for firmware update");
    } else {
        status = StoreOperationStatus_SUCCESS;
#ifdef FIRMWARE_SIGNING_ENABLED
        std::unordered_map<uint16_t, std::string> file_map;  // Map of LOD types and file names
        status = ValidateFirmwareSignature(tmp_format, &file_map, corrupt);
        unlink(tmp_format.c_str());
        if (status == StoreOperationStatus_SUCCESS) {
            LOG(INFO) << "Firmware is valid";
#if BUILD_FOR_ARM == 1
            // If there is a precheck, run it and make sure it is successful before starting download
            uint16_t precheck_type = kPRECHECK_LOD_TYPE;
            auto precheck_pair = file_map.find(precheck_type);
            if (precheck_pair != file_map.end() && !ExecutePrecheck(precheck_pair->second)) {
                LOG(WARNING) << "Precheck failed";
                command_response->mutable_status()->set_code(Command_Status_StatusCode::Command_Status_StatusCode_INTERNAL_ERROR);
                command_response->mutable_status()->set_statusmessage("Precheck failed");
                status = StoreOperationSTatus_PRECHECK_FAILED;
            } else {
                std::queue<std::string>* download_queue = CreateDownloadQueue(file_map);
                int state_result = server_->SupportableStateChanged(com::seagate::kinetic::StateEvent::DOWNLOAD,
                                                                proto::Message_AuthType_HMACAUTH,
                                                                command.header().messagetype(),
                                                                command_response,
                                                                download_queue);
                if (state_result < 0) {
                    command_response->mutable_status()->set_statusmessage("Unsupportable state changed");
                    status = StoreOperationStatus_UNSUPPORTABLE;
                }
            }
#endif
        } else {
            LOG(ERROR) << "Firmware is not valid";
            command_response->mutable_status()->set_statusmessage("Firmware download file is invalid");
        }
#else

        std::queue<std::string>* download_queue = new std::queue<std::string>;
        download_queue->push(tmp_format);
        int state_result = server_->SupportableStateChanged(com::seagate::kinetic::StateEvent::DOWNLOAD,
                                                            proto::Message_AuthType_HMACAUTH,
                                                            command.header().messagetype(),
                                                            command_response,
                                                            download_queue);
        if (state_result < 0) {
            command_response->mutable_status()->set_statusmessage("Unsupportable state changed");
            status =  StoreOperationStatus_UNSUPPORTABLE;
        }
#endif
    }
    return status;
}

StoreOperationStatus SetupHandler::ValidateFirmwareSignature(const string& firmware_file_path,
        std::unordered_map<uint16_t, std::string>* file_map, bool corrupt) {
    std::vector<struct LODHeader> headers;

    int res = ReadFirmwareFile(firmware_file_path, &headers, file_map);
    if (res == 1) {
        LOG(ERROR) << "Invalid firmware";//NO_SPELL
        return StoreOperationStatus_FIRMWARE_INVALID;
    } else if (res == 2) {
        LOG(ERROR) << "There was an error reading the file";
        return StoreOperationStatus_INTERNAL_ERROR;
    }

    // Validate each LOD Header checksum
    for (auto header = headers.begin(); header != headers.end(); ++header) {
        if (!VerifyLODHeaderChecksum(header)) {
            LOG(ERROR) << "Not a valid LOD Header";//NO_SPELL
            return StoreOperationStatus_FIRMWARE_INVALID;
        }
    }

    // Verify Block point, SHA256, and Security signature
    StoreOperationStatus status = StoreOperationStatus_SUCCESS;
    if (!VerifySHA256(headers, *file_map)) {
        LOG(ERROR) << "SHA256 is not valid";//NO_SPELL
        status = StoreOperationStatus_FIRMWARE_INVALID;
    } else if (!VerifySecuritySignature(headers)) {
        LOG(ERROR) << "Security signature not valid";
        status = StoreOperationStatus_FIRMWARE_INVALID;
    } else if (!corrupt && !VerifyKineticBlockPoint(headers, *file_map)) {
        LOG(ERROR) << "Block point does not match";
        status = StoreOperationStatus_BLOCK_POINT_MISMATCH;
    }

    return status;
}

bool SetupHandler::RemoveOldFirmwareFiles(const std::string& firmware_file_path) {
    std::stringstream command_stream;
    command_stream << "find "
                   << FLAGS_firmware_update_tmp_dir
                   << " -type f "
                   << " -exec rm {} \\;";

    if (system(command_stream.str().c_str()) != 0) {
        PLOG(ERROR) << "Delete command failed";
        return false;
    }

    return true;
}

int SetupHandler::ReadFirmwareFile(const string& firmware_file_path, std::vector<struct LODHeader>* headers,
        std::unordered_map<uint16_t, std::string>* file_map) {
    std::ifstream firmware_file_stream(firmware_file_path, std::ofstream::binary);
    if (!firmware_file_stream.is_open()) {
        LOG(ERROR) << "Unable to open firmware file";
        return 2;
    }

    uint32_t bytes_read = 0;
    uint32_t file_length = GetFileLength(&firmware_file_stream);
    while (bytes_read < file_length) {
        struct LODHeader header;
        bytes_read += ReadLODHeader(&header, &firmware_file_stream);

        // Check if payload size exceeds remaining file length
        if (header.input_size > file_length - bytes_read) {
            return 1;
        }

        // Validate Tag signature and exit early if unsuccessful
        if (!VerifyLODHeaderTagSignature(&header)) {
            return 1;
        }

        // If the input is a firmware binary, write it to disk
        if (header.signature == kKINETIC_LOD_SIGNATURE && (header.input_size != kTHUMBPRINT_SIZE ||
                header.type != kKINETIC_LOD_TYPE)) {
            VLOG(1) << "Found firmware binary of type " << static_cast<unsigned int>(header.type);

            // Check if there already is a file with this type in the map
            if (file_map->find(header.type) != file_map->end()) {
                // Already a file with this type, invalid firmware download
                VLOG(1) << "Multiple kinetic LOD headers with the same type detected";
                return 1;
            }

            std::string filename;
            if (!WriteFileToDisk(&firmware_file_stream, &header, &filename)) {
                return 2;
            }

            // Add file to map
            file_map->insert(std::make_pair(header.type, filename));
            bytes_read += header.input_size;
        } else {
            header.input.resize(header.input_size);
            firmware_file_stream.read(static_cast<char*>(&header.input[0]), header.input_size);
            bytes_read += firmware_file_stream.gcount();
        }

        headers->push_back(header);
    }

    // Make sure that the firmware file included firmware binaries
    if (file_map->empty()) {
        VLOG(1) << "No binaries were found in the firmware download";
        return 1;
    }

    return 0;
}

bool SetupHandler::WriteFileToDisk(std::ifstream* firmware_file_stream, struct LODHeader *lodHeader,
        std::string* file_path) {
    std::string tmp_format(firmware_update_tmp_dir_);
    tmp_format.append(kFIRMWARE_RUN_TEMPLATE);

    int tmp_fd = mkstemp(&tmp_format[0]);
    if (tmp_fd == -1) {
        PLOG(ERROR) << "Failed to create a temporary file for firmware update";
        return false;
    }

    std::string buffer;
    buffer.resize(kROLLING_WINDOW_SIZE);
    ReaderWriter reader_writer(tmp_fd);
    for (uint32_t bytes_read = 0; bytes_read < lodHeader->input_size; bytes_read += firmware_file_stream->gcount()) {
        uint32_t bytes_remaining = lodHeader->input_size - bytes_read;
        uint32_t to_copy = bytes_remaining > kROLLING_WINDOW_SIZE ? kROLLING_WINDOW_SIZE : bytes_remaining;

        // Read into buffer
        firmware_file_stream->read(static_cast<char*>(&buffer[0]), to_copy);

        // Check that we read the correct amount
        if (firmware_file_stream->gcount() < 0 || static_cast<uint32_t>(firmware_file_stream->gcount()) != to_copy) {
            LOG(ERROR) << "Only " << firmware_file_stream->gcount() << " bytes could be read";
            close(tmp_fd);
            return false;
        }

        // Write buffer to file
        if (!reader_writer.Write(static_cast<char*>(&buffer[0]), to_copy)) {
            PLOG(ERROR) << "Could not write firmware update to file";
            close(tmp_fd);
            return false;
        }
    }
    *file_path = tmp_format;
    close(tmp_fd);
    return true;
}

size_t SetupHandler::ReadLODHeader(struct LODHeader *lodHeader, std::ifstream* firmware_file_stream) {
    // Initialize header
    lodHeader->header.resize(kLODHEADER_SIZE);
    char* header = static_cast<char*>(&lodHeader->header[0]);
    firmware_file_stream->read(header, kLODHEADER_SIZE);

    // Populate signature, input size, block point, and checksum
    lodHeader->signature = GetLODHeaderSection(header, kSIGNATURE_OFFSET, kSIGNATURE_SIZE);
    lodHeader->type = GetLODHeaderSection(header, kTYPE_OFFSET, kTYPE_SIZE);
    lodHeader->block_point = GetLODHeaderSection(header, kBLOCK_POINT_OFFSET, kBLOCK_POINT_SIZE);
    lodHeader->input_size = GetLODHeaderSection(header, kINPUT_OFFSET, kINPUT_SIZE);
    lodHeader->checksum = GetLODHeaderSection(header, kCHECK_SUM_OFFSET, kCHECK_SUM_SIZE);

    return firmware_file_stream->gcount();
}

unsigned int SetupHandler::GetLODHeaderSection(char* lodHeader, unsigned int offset, unsigned int size) {
    unsigned int section = 0;
    unsigned int shift = 0;
    for (unsigned int idx = offset; idx < offset + size; ++idx, shift += 8) {
        section += (static_cast<unsigned int>(lodHeader[idx]) & 0xFF) << shift;
    }

    return section;
}

bool SetupHandler::VerifyLODHeaderChecksum(std::vector<struct LODHeader>::iterator lodHeader) {
    // Calculate checksum, use size - 2 since the last two bytes are the checksum value
    int checksum = CalculateCheckSum(static_cast<char*>(&lodHeader->header[0]), kLODHEADER_SIZE - 2);

    // Compare LOD checksum to calculated checksum
    if (checksum != lodHeader->checksum) {
        LOG(ERROR) << "Checksum does not match";
        return false;
    }

    return true;
}

int SetupHandler::CalculateCheckSum(char* lodHeader, unsigned int length) {
    int checksum = 0;
    for (unsigned int i = 0; i < length; i += 2) {
        checksum += GetLODHeaderSection(lodHeader, i, 2);
    }

    checksum = 0xFFFF - (checksum & 0xFFFF) + 1;
    checksum &= 0xFFFF;
    return checksum;
}

bool SetupHandler::VerifyLODHeaderTagSignature(struct LODHeader *lodHeader) {
    // Verify LOD tag signature
    if (kKINETIC_LOD_SIGNATURE != lodHeader->signature &&
        kSECURITY_INFO_LOD_SIGNATURE != lodHeader->signature &&
        kSECURITY_SIGNATURE_LOD_SIGNATURE != lodHeader->signature) {
        LOG(ERROR) << "LOD Signature does not match";//NO_SPELL
        return false;
    }
    return true;
}

bool SetupHandler::VerifyKineticBlockPoint(const std::vector<struct LODHeader>& headers,
        const std::unordered_map<uint16_t, std::string>& file_map) {
    for (auto it = file_map.begin(); it != file_map.end(); ++it) {
        int position = GetHeaderPosition(headers, it->first, false);

        // If value returned is -1, we chouldn't find the lod header, return false
        if (position == -1) {
            LOG(ERROR) << "Could not find header";
            return false;
        }
        // If block point does not match and db is not empty, return false
        assert(position >= 0 && position < (int)headers.size()); // NOLINT
        if (headers.at(position).block_point != kBLOCKPOINT) {
            float portion_full;
            device_information_.GetPortionFull(&portion_full);
            if (portion_full != 0) {
                return false;
            }
        }
    }
    return true;
}

bool SetupHandler::VerifySHA256(const std::vector<struct LODHeader>& headers,
        const std::unordered_map<uint16_t, std::string>& file_map) {
    // Get thumbprint position
    int position = GetHeaderPosition(headers, kKINETIC_LOD_TYPE, true);

    // Could not find the thumbprint data return false
    if (position == -1) {
        LOG(ERROR) << "Could not find thumbprint data";//NO_SPELL
        return false;
    }

    // Initalize SHA256
    SHA256_CTX context;
    if (!SHA256_Init(&context)) {
        LOG(ERROR) << "Failed to intialize SHA256";
        return false;
    }

    std::string buffer;
    buffer.resize(kROLLING_WINDOW_SIZE);

    // Loop to calculate SHA256 of concatenated firmware binaries
    for (auto header = headers.begin(); header != headers.end(); ++header) {
        // If LOD header does not belong to a firmware binary, continue
        if (header->signature != kKINETIC_LOD_SIGNATURE || (header->type == kKINETIC_LOD_TYPE &&
                header->input_size == kTHUMBPRINT_SIZE)) {
            continue;
        }

        // Find file in map
        auto file_pair = file_map.find(header->type);
        if (file_pair == file_map.end()) {
            LOG(ERROR) << "Firmware file of type " << static_cast<unsigned int>(header->type)
                       << " should exist in map but was not found";
            return false;
        }

        VLOG(2) << "Adding file '" << file_pair->second << "' of type " << static_cast<unsigned int>(file_pair->first)
                << " to SHA256 calculation";

        std::ifstream firmware_file_stream(file_pair->second, std::ofstream::binary);
        if (!firmware_file_stream.is_open()) {
            LOG(ERROR) << "Could not read firmware file";
            return false;
        }

        uint32_t file_size = GetFileLength(&firmware_file_stream);
        for (uint32_t bytes_read = 0; bytes_read < file_size; bytes_read += firmware_file_stream.gcount()) {
            uint32_t bytes_remaining = file_size - bytes_read;
            uint32_t to_read = bytes_remaining > kROLLING_WINDOW_SIZE ? kROLLING_WINDOW_SIZE : bytes_remaining;

            // Read into buffer
            firmware_file_stream.read(static_cast<char*>(&buffer[0]), to_read);

            // Check that we read the correct amount
            if (firmware_file_stream.gcount() < 0 || static_cast<uint32_t>(firmware_file_stream.gcount()) != to_read) {
                LOG(ERROR) << "Only " << firmware_file_stream.gcount() << " bytes could be read";
                return false;
            }

            // Update SHA with buffer contents
            if (!SHA256_Update(&context, static_cast<char*>(&buffer[0]), to_read)) {
                LOG(ERROR) << "SHA256 update failed";
                return false;
            }
        }
    }

    unsigned char sha[SHA256_DIGEST_LENGTH];
    if (!SHA256_Final(sha, &context)) {
        LOG(ERROR) << "SHA256 final failed";
        return false;
    }
    assert(position >= 0 && position < (int)headers.size()); // NOLINT
    struct LODHeader thumbprint_header = headers.at(position);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        if (sha[i] != (0xFF & thumbprint_header.input[i])) {
            VLOG(1) << "Calculated SHA256 did not match thumbprint";
            return false;
        }
    }

    return true;
}

bool SetupHandler::VerifySecuritySignature(const std::vector<struct LODHeader>& headers) {
    struct LODHeader thumbprint_lodHeader, security_signature, security_info;
    int thumbprint_position = GetHeaderPosition(headers, kKINETIC_LOD_TYPE, true);
    int security_signature_position = GetHeaderPosition(headers, kSECURITY_SIGNATURE_LOD_SIGNATURE);
    int security_info_position = GetHeaderPosition(headers, kSECURITY_INFO_LOD_SIGNATURE);

    // If we could not find all the headers, we can't verify the security signature, return false
    if (thumbprint_position == -1 || security_signature_position == -1 || security_info_position == -1) {
        LOG(ERROR) << "Could not find header";
        return false;
    }

    // Set headers
    assert(thumbprint_position >= 0 && thumbprint_position < (int)headers.size()); // NOLINT
    thumbprint_lodHeader = headers.at(thumbprint_position);
    assert(security_signature_position >= 0 && security_signature_position < (int)headers.size()); // NOLINT
    security_signature = headers.at(security_signature_position);
    assert(security_info_position >= 0 && security_info_position < (int)headers.size()); // NOLINT
    security_info = headers.at(security_info_position);

    return security_handler_.VerifySecuritySignature(&thumbprint_lodHeader, &security_signature, &security_info);
}

uint32_t SetupHandler::GetFileLength(std::ifstream* firmware_file_stream) {
    firmware_file_stream->seekg(0, firmware_file_stream->end);
    int64_t file_length = firmware_file_stream->tellg();
    firmware_file_stream->seekg(0, firmware_file_stream->beg);
    return file_length > 0 ? static_cast<uint32_t>(file_length) : 0;
}

int SetupHandler::GetHeaderPosition(const std::vector<struct LODHeader>& headers, const uint32_t tag_signature) {
    int position = 0;
    for (auto header = headers.begin(); header != headers.end(); ++header, ++position) {
        if (header->signature == tag_signature) {
            return position;
        }
    }
    // Could not find the header
    return -1;
}

int SetupHandler::GetHeaderPosition(const std::vector<struct LODHeader>& headers, const uint8_t type, bool thumbprint) {
    int position = 0;
    for (auto header = headers.begin(); header != headers.end(); ++header, ++position) {
        if (header->signature == kKINETIC_LOD_SIGNATURE && header->type == type) {
            if (!thumbprint || header->input_size == kTHUMBPRINT_SIZE) {
                return position;
            }
        }
    }
    return -1;
}

bool SetupHandler::ExecutePrecheck(const std::string& filename) {
    int ret_code;
    std::string command_result;
    RawStringProcessor processor(&command_result, &ret_code);

    std::string command = "/bin/sh " + filename;
    if (!execute_command(command, processor)) {
        VLOG(1) << "Precheck output:\n" << command_result;
        return false;
    } else {
        return true;
    }
}

std::queue<std::string>* SetupHandler::CreateDownloadQueue(const std::unordered_map<uint16_t, std::string>& file_map) {
    std::queue<std::string>* download_queue = new std::queue<std::string>;
    std::vector<uint16_t> download_order = {kF3_LOD_TYPE, kUBOOT_LOD_TYPE, kKINETIC_LOD_TYPE};

    for (auto type = download_order.begin(); type != download_order.end(); ++type) {
        auto it = file_map.find(*type);
        if (it != file_map.end()) {
            download_queue->push(it->second);
        }
    }

    return download_queue;
}

} // namespace kinetic
} // namespace seagate
} // namespace com
