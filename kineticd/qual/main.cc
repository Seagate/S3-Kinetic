#include <string>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <fcntl.h>
#include "zac_mediator.h"
#include "includes/drive_helper.h"

const std::string kATI_STE = "ati_ste";

int main() {
    int read_fd = open(qual_kin::kDEVICE_NAME, (O_DIRECT | O_RDWR));
    if (read_fd < 0) {
        std::cout << "Bad file descriptor" << std::endl;
        return -1;
    }

    AtaCmdHandler ata;
    ZacMediator zac(&ata);
    zac.OpenDevice(qual_kin::kDEVICE_NAME);

    qual_kin::test_read_write(read_fd, zac);
    qual_kin::test_write_zone(read_fd, zac);
}
