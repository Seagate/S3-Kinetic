/*
 * kinetic-cpp-client
 * Copyright (C) 2014 Seagate Technology.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <stdio.h>

#include "kinetic/kinetic.h"
#include "gflags/gflags.h"

using kinetic::Status;
using kinetic::KineticStatus;
using kinetic::KineticRecord;

using std::shared_ptr;
using std::string;
using std::unique_ptr;

int main(int argc, char *argv[]) {
    unique_ptr<kinetic::BlockingKineticConnection> blocking_connection_(nullptr);
    kinetic::ConnectionOptions options;
    options.host = "localhost";
    options.port = 8443;
    options.use_ssl = true;
    options.user_id = 1;
    options.hmac_key = "asdfasdf";

    kinetic::KineticConnectionFactory connection_factory = kinetic::NewKineticConnectionFactory();
    connection_factory.NewBlockingConnection(options,blocking_connection_, 10);

    KineticStatus status = blocking_connection_->InstantErase("");

    if (!status.ok()) {
        printf("Unable to execute InstantErase: %d %s\n", static_cast<int>(status.statusCode()), status.message().c_str());
        return 1;
    }

    printf("Finished ISE\n");

    return 0;
}