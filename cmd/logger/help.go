/*
 * MinIO Cloud Storage, (C) 2019 MinIO, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package logger

import "github.com/minio/minio/cmd/config"

// Help template for logger http and audit
var (
	Help = config.HelpKVS{
		config.HelpKV{
			Key:         Endpoint,
			Description: `HTTP logger endpoint eg: "http://localhost:8080/minio/logs/server"`,
			Type:        "url",
		},
		config.HelpKV{
			Key:         AuthToken,
			Description: "Authorization token for logger endpoint",
			Optional:    true,
			Type:        "string",
		},
		config.HelpKV{
			Key:         config.Comment,
			Description: "A comment to describe the HTTP logger setting",
			Optional:    true,
			Type:        "sentence",
		},
	}

	HelpAudit = config.HelpKVS{
		config.HelpKV{
			Key:         Endpoint,
			Description: `HTTP Audit logger endpoint eg: "http://localhost:8080/minio/logs/audit"`,
			Type:        "url",
		},
		config.HelpKV{
			Key:         AuthToken,
			Description: "Authorization token for logger endpoint",
			Optional:    true,
			Type:        "string",
		},
		config.HelpKV{
			Key:         config.Comment,
			Description: "A comment to describe the HTTP Audit logger setting",
			Optional:    true,
			Type:        "sentence",
		},
	}
)
