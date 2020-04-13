#ifndef KINETIC_SETUP_HANDLER_H_
#define KINETIC_SETUP_HANDLER_H_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <queue>

#include "authenticator_interface.h"
#include "cluster_version_store.h"
#include "device_information.h"
#include "kinetic.pb.h"
#include "kinetic/reader_writer.h"
#include "security_handler.h"
#include "skinny_waist.h"

namespace com {
namespace seagate {
namespace kinetic {

using ::kinetic::ReaderWriter;

class Server;

class SetupHandler {
 public:
    explicit SetupHandler(
            AuthorizerInterface& authorizer,
            SkinnyWaistInterface& skinny_waist,
            ClusterVersionStoreInterface& cluster_version_store,
            const std::string& firmware_update_tmp_dir,
            SecurityHandlerInterface& security_handler,
            DeviceInformationInterface& device_information);
    void ProcessRequest(const proto::Command &command,
                        IncomingValueInterface* request_value,
                        proto::Command *command_response,
                        RequestContext& request_context,
                        uint64_t userid,
                        bool corrupt = false);
    void ProcessPinRequest(const proto::Command &command,
                            IncomingValueInterface* request_value,
                            proto::Command *command_response,
                            RequestContext& request_context,
                            const std::string& pin);
    void SetServer(Server* server) {
        server_ = server;
    }

 private:
    StoreOperationStatus HandleSetClusterVersion(const proto::Command &command);
    StoreOperationStatus HandleFirmwareDownload(const proto::Command &command,
                                                IncomingValueInterface* request_value,
                                                proto::Command* command_response,
                                                bool corrupt = false);
    StoreOperationStatus ValidateFirmwareSignature(const std::string& firmware_file_path,
                                                   std::unordered_map<uint16_t, std::string>* file_map,
                                                   bool corrupt = false);
    int ReadFirmwareFile(const string& firmware_file_path,
                         std::vector<struct LODHeader>* headers,
                         std::unordered_map<uint16_t, std::string>* file_map);
    bool WriteFileToDisk(std::ifstream* firmware_file_stream, struct LODHeader* lodHeader, std::string* file_path);
    bool VerifyLODHeaderChecksum(std::vector<struct LODHeader>::iterator it);
    bool VerifyLODHeaderTagSignature(struct LODHeader* lodHeader);
    bool VerifyKineticBlockPoint(const std::vector<struct LODHeader>& headers,
                                 const std::unordered_map<uint16_t, std::string>& file_map);
    bool VerifySHA256(const std::vector<struct LODHeader>& headers,
                      const std::unordered_map<uint16_t, std::string>& file_map);
    bool VerifySecuritySignature(const std::vector<struct LODHeader>& headers);
    size_t ReadLODHeader(struct LODHeader* lodHeader, std::ifstream* firmware_file_stream);
    unsigned int GetLODHeaderSection(char* lodHeader, unsigned int offset, unsigned int size);
    int CalculateCheckSum(char* lodHeader, unsigned int length);
    uint32_t GetFileLength(std::ifstream* firmware_file_stream);
    bool RemoveOldFirmwareFiles(const std::string& firmware_file_path);
    int GetHeaderPosition(const std::vector<struct LODHeader>& headers, const uint32_t tag_signature);
    int GetHeaderPosition(const std::vector<struct LODHeader>& headers, const uint8_t type, bool thumbprint);
    bool ExecutePrecheck(const std::string& filename);
    std::queue<std::string>* CreateDownloadQueue(const std::unordered_map<uint16_t, std::string>& file_map);


    // Constants for LOD Header structure/fields
    static const uint32_t kKINETIC_LOD_SIGNATURE = 0x24280378;
    static const uint32_t kSECURITY_INFO_LOD_SIGNATURE = 0x590E1AE7;
    static const uint32_t kSECURITY_SIGNATURE_LOD_SIGNATURE = 0xA6942905;
    static const uint16_t kBLOCKPOINT = 0x0002;
    static const uint16_t kPRECHECK_LOD_TYPE = 0x18;
    static const uint16_t kUBOOT_LOD_TYPE = 0x19;
    static const uint16_t kF3_LOD_TYPE = 0x1A;
    static const uint16_t kKINETIC_LOD_TYPE = 0x1B;
    static const unsigned int kTHUMBPRINT_SIZE = 256;
    static const unsigned int kLODHEADER_SIZE = 0x40;
    static const unsigned int kSIGNATURE_SIZE = 4;
    static const unsigned int kSIGNATURE_OFFSET = 0;
    static const unsigned int kBLOCK_POINT_SIZE = 2;
    static const unsigned int kBLOCK_POINT_OFFSET = 4;
    static const unsigned int kTYPE_OFFSET = 14;
    static const unsigned int kTYPE_SIZE = 2;
    static const unsigned int kINPUT_SIZE = 4;
    static const unsigned int kINPUT_OFFSET = 16;
    static const unsigned int kCHECK_SUM_SIZE = 2;
    static const unsigned int kCHECK_SUM_OFFSET = 62;

    // Constant for setup handler behavior
    static const unsigned int kROLLING_WINDOW_SIZE = 10000000;

    // Constants for file templates
    static const char kFIRMWARE_UPDATE_TEMPLATE[];
    static const char kFIRMWARE_RUN_TEMPLATE[];

    AuthorizerInterface& authorizer_;
    SkinnyWaistInterface& skinny_waist_;
    ClusterVersionStoreInterface& cluster_version_store_;
    const std::string firmware_update_tmp_dir_;
    SecurityHandlerInterface& security_handler_;
    DeviceInformationInterface& device_information_;
    Server* server_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_SETUP_HANDLER_H_
