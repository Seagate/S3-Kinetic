#ifndef KINETIC_HA_ZAC_CMDS_INCLUDES_ATA_CMD_HANDLER_H_
#define KINETIC_HA_ZAC_CMDS_INCLUDES_ATA_CMD_HANDLER_H_
#include <sys/types.h>
#include <iostream>
#include <vector>
#include <scsi/sg.h>
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
/// AtaCmdHandler -- Helper Class for Kinetic Host Aware Command Set
///     - Used By: @ZacMediator
/// -------------------------------------------------------------------------------
/// @Summary:
/// - Used by @ZacMediator to send Host-Aware ATA Commands to HA device
/// - Commands are wrapped w/ SCSI Generic Headers
/// - This class is only concerned with formatting and executing SCSI Generic commands
///   The nature and content of the individual commands are handled by @ZacMediator
/// - Header structure is defined by linux sg driver
/// - SCSI commands are 6, 10, 12 or 16 bytes long
/// -------------------------------------------------------------------------------
/// @Member Variables
/// - @data_buf_    -- holds Scsi Generic Command Results (i.e. data returned from read op)
/// - @sense_buf_   -- holds resulting SCSI sense data from legacy device
///                    The SCSI sense buffer is the error reporting facility for SCSI
/// -------------------------------------------------------------------------------
namespace zac_ha_cmd {
class AtaCmdHandler {
    public:
    /// Helper Variables for Command Result and Sense Data Formatting / Retrieval
    static const unsigned int kZAC_SG_ATA16_CDB_OPCODE_ = 0x85;
    static const unsigned int kZAC_SG_SENSE_MAX_LENGTH = 64;
    static const unsigned int kSG_CHECK_CONDITION_ = 0x02;
    static const unsigned int kSG_DID_OK_ = 0x00;

    /////////////////////////////////////////////////////////
    /// Constructor clears @data_buf_ vector on creation
    AtaCmdHandler();

    /////////////////////////////////////////////////////////
    /// Destructor clears @data_buf_ vector on creation
    ~AtaCmdHandler();

    ////////////////////////////////////////////////////////////////////////////////////
    /// ConstructIoHeader(dxfer_direct, *io_hdr)
    /// - Caller: @ZacMediator
    /// -------------------------------------------------------------------------------
    /// populate sg_io_hdr with SCSI Generic fields required for command execution
    /// -------------------------------------------------------------------------------
    /// - @param[in] dxfer_direct -- data transfer direction for this cmd
    ///                               (e.g. a READ command will specify a dxfer_direct of SG_DXFER_FROM_DEV)
    /// - @param[in] *io_hdr      -- Pointer to scsi generic header created by @ZacMediator at cmd execution time.
    ///                               This will be populated with SG specific fields
    /// - @return[out] int        -- Status indicating successful / failed population of IO header @io_hdr
    /// -------------------------------------------------------------------------------
    int ConstructIoHeader(int dxfer_direct, sg_io_hdr_t *io_hdr);

    ////////////////////////////////////////////////////////////////////////////////////
    /// ConstructIoHeader(dxfer_direct, *io_hdr, *data, data_size)
    /// - Caller: @ZacMediator
    /// -------------------------------------------------------------------------------
    /// populate sg_io_hdr with SCSI Generic fields required for command execution
    /// -------------------------------------------------------------------------------
    /// - @param[in] dxfer_direct -- data transfer direction for this cmd
    ///                              (e.g. a READ command will specify a dxfer_direct of SG_DXFER_FROM_DEV)
    /// - @param[in] *io_hdr      -- Pointer to scsi generic header created by @ZacMediator at cmd execution time.
    ///                              This will be populated with SG specific fields
    /// - @param[in] *data        -- Pointer to data buffer to be transfered to device.
    ///                              The dxferp field within the io_header will be assigned to this buffer address
    /// - @param[in] *data_size   -- size of @data buffer
    /// - @return[out] int        -- Status indicating successful / failed population of IO header @io_hdr
    /// -------------------------------------------------------------------------------
    int ConstructIoHeader(int dxfer_direct, sg_io_hdr_t *io_hdr, uint8_t *data, size_t data_size);

    ////////////////////////////////////////////////////////////////////////////////////
    /// ExecuteSgCmd(fd, *cdb, cdb_length, io_hdr)
    /// - Caller: @ZacMediator
    /// -------------------------------------------------------------------------------
    /// Execute SCSI Command defined within the CDB
    /// -------------------------------------------------------------------------------
    /// - @param[in] fd         -- device file descriptor. Passed to the SCSI ioctl()
    /// - @param[in] *cdb       -- Command to be executed. Ptr to populated cmd struct created in ZacMediator PopulateCdb().
    ///                            Will be assigned to the io_hdr's "cmdp" field
    /// - @param[in] cdb_length -- length of the SCSI command structure (CDB)
    /// - @param[in] io_hdr     -- Pointer to SCSI generic header struct
    /// - @return[out] int      -- return status of ioctl command
    /// -------------------------------------------------------------------------------
    int ExecuteSgCmd(int fd, uint8_t *cdb, size_t cdb_length, sg_io_hdr_t io_hdr);

    /// Return Pointer to @data_buf_ vector
    std::vector<uint8_t>* GetDataBufp();

    /// Return Pointer to @sense_buf_ array
    uint8_t* GetSenseBufp();

    /// Allocate @data_buf_ vector with @size elements of value 0
    void AllocateDataBuf(unsigned int size);

    /// Remove all elements from the @data_buf_ vector
    void ClearDataBuf();

    private:
    std::vector<uint8_t> data_buf_;
    uint8_t sense_buf_[kZAC_SG_SENSE_MAX_LENGTH];
};

} // namespace zac_ha_cmd

#endif  // KINETIC_HA_ZAC_CMDS_INCLUDES_ATA_CMD_HANDLER_H_
