/*
 * MinIO Cloud Storage, (C) 2016-2019 MinIO, Inc.
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

package cmd

import (
	"context"
	"crypto/subtle"
	"encoding/json"
	"fmt"
	"io"
	"io/ioutil"
	"net/http"
	"net/url"
	"os"
	"path"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"time"

	humanize "github.com/dustin/go-humanize"
	"github.com/gorilla/mux"

	"github.com/minio/minio/cmd/crypto"
	xhttp "github.com/minio/minio/cmd/http"
	"github.com/minio/minio/cmd/logger"
	"github.com/minio/minio/pkg/cpu"
	"github.com/minio/minio/pkg/handlers"
	iampolicy "github.com/minio/minio/pkg/iam/policy"
	"github.com/minio/minio/pkg/madmin"
	"github.com/minio/minio/pkg/mem"
	xnet "github.com/minio/minio/pkg/net"
	"github.com/minio/minio/pkg/quick"
	trace "github.com/minio/minio/pkg/trace"
)

const (
	maxEConfigJSONSize = 262272
	defaultNetPerfSize = 100 * humanize.MiByte
)

// Type-safe query params.
type mgmtQueryKey string

// Only valid query params for mgmt admin APIs.
const (
	mgmtBucket      mgmtQueryKey = "bucket"
	mgmtPrefix                   = "prefix"
	mgmtClientToken              = "clientToken"
	mgmtForceStart               = "forceStart"
	mgmtForceStop                = "forceStop"
)

func updateServer(updateURL, sha256Hex string, latestReleaseTime time.Time) (us madmin.ServerUpdateStatus, err error) {
	minioMode := getMinioMode()
	// No inputs provided we should try to update using the default URL.
	if updateURL == "" && sha256Hex == "" && latestReleaseTime.IsZero() {
		var updateMsg string
		updateMsg, sha256Hex, _, latestReleaseTime, err = getUpdateInfo(updateTimeout, minioMode)
		if err != nil {
			return us, err
		}
		if updateMsg == "" {
			us.CurrentVersion = Version
			us.UpdatedVersion = Version
			return us, nil
		}
		if runtime.GOOS == "windows" {
			updateURL = minioReleaseURL + "minio.exe"
		} else {
			updateURL = minioReleaseURL + "minio"
		}
	}
	if err = doUpdate(updateURL, sha256Hex, minioMode); err != nil {
		return us, err
	}
	us.CurrentVersion = Version
	us.UpdatedVersion = latestReleaseTime.Format(minioReleaseTagTimeLayout)
	return us, nil
}

// ServerUpdateHandler - POST /minio/admin/v1/update?updateURL={updateURL}
// ----------
// updates all minio servers and restarts them gracefully.
func (a adminAPIHandlers) ServerUpdateHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "ServerUpdate")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	if globalInplaceUpdateDisabled {
		// if MINIO_UPDATE=off - inplace update is disabled, mostly
		// in containers.
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrMethodNotAllowed), r.URL)
		return
	}

	vars := mux.Vars(r)
	updateURL := vars[peerRESTUpdateURL]
	mode := getMinioMode()
	var sha256Hex string
	var latestReleaseTime time.Time
	if updateURL != "" {
		u, err := url.Parse(updateURL)
		if err != nil {
			writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
			return
		}

		content, err := downloadReleaseURL(updateURL, updateTimeout, mode)
		if err != nil {
			writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
			return
		}

		sha256Hex, latestReleaseTime, err = parseReleaseData(content)
		if err != nil {
			writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
			return
		}

		if runtime.GOOS == "windows" {
			u.Path = path.Dir(u.Path) + "minio.exe"
		} else {
			u.Path = path.Dir(u.Path) + "minio"
		}

		updateURL = u.String()
	}

	for _, nerr := range globalNotificationSys.ServerUpdate(updateURL, sha256Hex, latestReleaseTime) {
		if nerr.Err != nil {
			logger.GetReqInfo(ctx).SetTags("peerAddress", nerr.Host.String())
			logger.LogIf(ctx, nerr.Err)
		}
	}

	updateStatus, err := updateServer(updateURL, sha256Hex, latestReleaseTime)
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	// Marshal API response
	jsonBytes, err := json.Marshal(updateStatus)
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	writeSuccessResponseJSON(w, jsonBytes)

	if updateStatus.CurrentVersion != updateStatus.UpdatedVersion {
		// We did upgrade - restart all services.
		globalServiceSignalCh <- serviceRestart
	}
}

// ServiceActionHandler - POST /minio/admin/v1/service?action={action}
// ----------
// restarts/stops minio server gracefully. In a distributed setup,
func (a adminAPIHandlers) ServiceActionHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "ServiceAction")

	vars := mux.Vars(r)
	action := vars["action"]

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	var serviceSig serviceSignal
	switch madmin.ServiceAction(action) {
	case madmin.ServiceActionRestart:
		serviceSig = serviceRestart
	case madmin.ServiceActionStop:
		serviceSig = serviceStop
	default:
		logger.LogIf(ctx, fmt.Errorf("Unrecognized service action %s requested", action))
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrMalformedPOSTRequest), r.URL)
		return
	}

	// Notify all other MinIO peers signal service.
	for _, nerr := range globalNotificationSys.SignalService(serviceSig) {
		if nerr.Err != nil {
			logger.GetReqInfo(ctx).SetTags("peerAddress", nerr.Host.String())
			logger.LogIf(ctx, nerr.Err)
		}
	}

	// Reply to the client before restarting, stopping MinIO server.
	writeSuccessResponseHeadersOnly(w)

	globalServiceSignalCh <- serviceSig
}

// ServerProperties holds some server information such as, version, region
// uptime, etc..
type ServerProperties struct {
	Uptime       time.Duration `json:"uptime"`
	Version      string        `json:"version"`
	CommitID     string        `json:"commitID"`
	DeploymentID string        `json:"deploymentID"`
	Region       string        `json:"region"`
	SQSARN       []string      `json:"sqsARN"`
}

// ServerConnStats holds transferred bytes from/to the server
type ServerConnStats struct {
	TotalInputBytes  uint64 `json:"transferred"`
	TotalOutputBytes uint64 `json:"received"`
	Throughput       uint64 `json:"throughput,omitempty"`
}

// ServerHTTPMethodStats holds total number of HTTP operations from/to the server,
// including the average duration the call was spent.
type ServerHTTPMethodStats struct {
	Count       uint64 `json:"count"`
	AvgDuration string `json:"avgDuration"`
}

// ServerHTTPStats holds all type of http operations performed to/from the server
// including their average execution time.
type ServerHTTPStats struct {
	TotalHEADStats     ServerHTTPMethodStats `json:"totalHEADs"`
	SuccessHEADStats   ServerHTTPMethodStats `json:"successHEADs"`
	TotalGETStats      ServerHTTPMethodStats `json:"totalGETs"`
	SuccessGETStats    ServerHTTPMethodStats `json:"successGETs"`
	TotalPUTStats      ServerHTTPMethodStats `json:"totalPUTs"`
	SuccessPUTStats    ServerHTTPMethodStats `json:"successPUTs"`
	TotalPOSTStats     ServerHTTPMethodStats `json:"totalPOSTs"`
	SuccessPOSTStats   ServerHTTPMethodStats `json:"successPOSTs"`
	TotalDELETEStats   ServerHTTPMethodStats `json:"totalDELETEs"`
	SuccessDELETEStats ServerHTTPMethodStats `json:"successDELETEs"`
}

// ServerInfoData holds storage, connections and other
// information of a given server.
type ServerInfoData struct {
	StorageInfo StorageInfo      `json:"storage"`
	ConnStats   ServerConnStats  `json:"network"`
	HTTPStats   ServerHTTPStats  `json:"http"`
	Properties  ServerProperties `json:"server"`
}

// ServerInfo holds server information result of one node
type ServerInfo struct {
	Error string          `json:"error"`
	Addr  string          `json:"addr"`
	Data  *ServerInfoData `json:"data"`
}

// ServerInfoHandler - GET /minio/admin/v1/info
// ----------
// Get server information
func (a adminAPIHandlers) ServerInfoHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "ServerInfo")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	serverInfo := globalNotificationSys.ServerInfo(ctx)
	// Once we have received all the ServerInfo from peers
	// add the local peer server info as well.
	serverInfo = append(serverInfo, ServerInfo{
		Addr: getHostName(r),
		Data: &ServerInfoData{
			StorageInfo: objectAPI.StorageInfo(ctx),
			ConnStats:   globalConnStats.toServerConnStats(),
			HTTPStats:   globalHTTPStats.toServerHTTPStats(),
			Properties: ServerProperties{
				Uptime:       UTCNow().Sub(globalBootTime),
				Version:      Version,
				CommitID:     CommitID,
				DeploymentID: globalDeploymentID,
				SQSARN:       globalNotificationSys.GetARNList(),
				Region:       globalServerConfig.GetRegion(),
			},
		},
	})

	// Marshal API response
	jsonBytes, err := json.Marshal(serverInfo)
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	// Reply with storage information (across nodes in a
	// distributed setup) as json.
	writeSuccessResponseJSON(w, jsonBytes)
}

// ServerCPULoadInfo holds informantion about cpu utilization
// of one minio node. It also reports any errors if encountered
// while trying to reach this server.
type ServerCPULoadInfo struct {
	Addr         string     `json:"addr"`
	Error        string     `json:"error,omitempty"`
	Load         []cpu.Load `json:"load"`
	HistoricLoad []cpu.Load `json:"historicLoad"`
}

// ServerMemUsageInfo holds informantion about memory utilization
// of one minio node. It also reports any errors if encountered
// while trying to reach this server.
type ServerMemUsageInfo struct {
	Addr          string      `json:"addr"`
	Error         string      `json:"error,omitempty"`
	Usage         []mem.Usage `json:"usage"`
	HistoricUsage []mem.Usage `json:"historicUsage"`
}

// ServerNetReadPerfInfo network read performance information.
type ServerNetReadPerfInfo struct {
	Addr           string `json:"addr"`
	ReadThroughput uint64 `json:"readThroughput"`
	Error          string `json:"error,omitempty"`
}

// PerfInfoHandler - GET /minio/admin/v1/performance?perfType={perfType}
// ----------
// Get all performance information based on input type
// Supported types = drive
func (a adminAPIHandlers) PerfInfoHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "PerfInfo")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	vars := mux.Vars(r)
	switch perfType := vars["perfType"]; perfType {
	case "net":
		var size int64 = defaultNetPerfSize
		if sizeStr, found := vars["size"]; found {
			var err error
			if size, err = strconv.ParseInt(sizeStr, 10, 64); err != nil || size < 0 {
				writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrBadRequest), r.URL)
				return
			}
		}

		if !globalIsDistXL {
			writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrMethodNotAllowed), r.URL)
			return
		}

		addr := r.Host
		if globalIsDistXL {
			addr = GetLocalPeer(globalEndpoints)
		}

		infos := map[string][]ServerNetReadPerfInfo{}
		infos[addr] = globalNotificationSys.NetReadPerfInfo(size)
		for peer, info := range globalNotificationSys.CollectNetPerfInfo(size) {
			infos[peer] = info
		}

		// Marshal API response
		jsonBytes, err := json.Marshal(infos)
		if err != nil {
			writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
			return
		}

		// Reply with performance information (across nodes in a
		// distributed setup) as json.
		writeSuccessResponseJSON(w, jsonBytes)

	case "drive":
		// Drive Perf is only implemented for Erasure coded backends
		if !globalIsXL {
			writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrMethodNotAllowed), r.URL)
			return
		}

		var size int64 = madmin.DefaultDrivePerfSize
		if sizeStr, found := vars["size"]; found {
			var err error
			if size, err = strconv.ParseInt(sizeStr, 10, 64); err != nil || size <= 0 {
				writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrBadRequest), r.URL)
				return
			}
		}
		// Get drive performance details from local server's drive(s)
		dp := getLocalDrivesPerf(globalEndpoints, size, r)

		// Notify all other MinIO peers to report drive performance numbers
		dps := globalNotificationSys.DrivePerfInfo(size)
		dps = append(dps, dp)

		// Marshal API response
		jsonBytes, err := json.Marshal(dps)
		if err != nil {
			writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
			return
		}

		// Reply with performance information (across nodes in a
		// distributed setup) as json.
		writeSuccessResponseJSON(w, jsonBytes)
	case "cpu":
		// Get CPU load details from local server's cpu(s)
		cpu := getLocalCPULoad(globalEndpoints, r)
		// Notify all other MinIO peers to report cpu load numbers
		cpus := globalNotificationSys.CPULoadInfo()
		cpus = append(cpus, cpu)

		// Marshal API response
		jsonBytes, err := json.Marshal(cpus)
		if err != nil {
			writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
			return
		}

		// Reply with cpu load information (across nodes in a
		// distributed setup) as json.
		writeSuccessResponseJSON(w, jsonBytes)
	case "mem":
		// Get mem usage details from local server(s)
		m := getLocalMemUsage(globalEndpoints, r)
		// Notify all other MinIO peers to report mem usage numbers
		mems := globalNotificationSys.MemUsageInfo()
		mems = append(mems, m)

		// Marshal API response
		jsonBytes, err := json.Marshal(mems)
		if err != nil {
			writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
			return
		}

		// Reply with mem usage information (across nodes in a
		// distributed setup) as json.
		writeSuccessResponseJSON(w, jsonBytes)
	default:
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrMethodNotAllowed), r.URL)
	}
}

func newLockEntry(l lockRequesterInfo, resource, server string) *madmin.LockEntry {
	entry := &madmin.LockEntry{Timestamp: l.Timestamp, Resource: resource, ServerList: []string{server}, Owner: l.Node, Source: l.Source, ID: l.UID}
	if l.Writer {
		entry.Type = "Write"
	} else {
		entry.Type = "Read"
	}
	return entry
}

func topLockEntries(peerLocks []*PeerLocks) madmin.LockEntries {
	entryMap := make(map[string]*madmin.LockEntry)
	for _, peerLock := range peerLocks {
		if peerLock == nil {
			continue
		}
		for k, v := range peerLock.Locks {
			for _, lockReqInfo := range v {
				if val, ok := entryMap[lockReqInfo.UID]; ok {
					val.ServerList = append(val.ServerList, peerLock.Addr)
				} else {
					entryMap[lockReqInfo.UID] = newLockEntry(lockReqInfo, k, peerLock.Addr)
				}
			}
		}
	}
	var lockEntries = make(madmin.LockEntries, 0)
	for _, v := range entryMap {
		lockEntries = append(lockEntries, *v)
	}
	sort.Sort(lockEntries)
	const listCount int = 10
	if len(lockEntries) > listCount {
		lockEntries = lockEntries[:listCount]
	}
	return lockEntries
}

// PeerLocks holds server information result of one node
type PeerLocks struct {
	Addr  string
	Locks GetLocksResp
}

// TopLocksHandler Get list of locks in use
func (a adminAPIHandlers) TopLocksHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "TopLocks")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	// Method only allowed in Distributed XL mode.
	if !globalIsDistXL {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrMethodNotAllowed), r.URL)
		return
	}

	peerLocks := globalNotificationSys.GetLocks(ctx)
	// Once we have received all the locks currently used from peers
	// add the local peer locks list as well.
	localLocks := globalLockServer.ll.DupLockMap()
	peerLocks = append(peerLocks, &PeerLocks{
		Addr:  getHostName(r),
		Locks: localLocks,
	})

	topLocks := topLockEntries(peerLocks)

	// Marshal API response
	jsonBytes, err := json.Marshal(topLocks)
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	// Reply with storage information (across nodes in a
	// distributed setup) as json.
	writeSuccessResponseJSON(w, jsonBytes)
}

// StartProfilingResult contains the status of the starting
// profiling action in a given server
type StartProfilingResult struct {
	NodeName string `json:"nodeName"`
	Success  bool   `json:"success"`
	Error    string `json:"error"`
}

// StartProfilingHandler - POST /minio/admin/v1/profiling/start?profilerType={profilerType}
// ----------
// Enable server profiling
func (a adminAPIHandlers) StartProfilingHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "StartProfiling")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	vars := mux.Vars(r)
	profiler := vars["profilerType"]

	thisAddr, err := xnet.ParseHost(GetLocalPeer(globalEndpoints))
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	// Start profiling on remote servers.
	hostErrs := globalNotificationSys.StartProfiling(profiler)

	// Start profiling locally as well.
	{
		if globalProfiler != nil {
			globalProfiler.Stop()
		}
		prof, err := startProfiler(profiler, "")
		if err != nil {
			hostErrs = append(hostErrs, NotificationPeerErr{
				Host: *thisAddr,
				Err:  err,
			})
		} else {
			globalProfiler = prof
			hostErrs = append(hostErrs, NotificationPeerErr{
				Host: *thisAddr,
			})
		}
	}

	var startProfilingResult []StartProfilingResult

	for _, nerr := range hostErrs {
		result := StartProfilingResult{NodeName: nerr.Host.String()}
		if nerr.Err != nil {
			result.Error = nerr.Err.Error()
		} else {
			result.Success = true
		}
		startProfilingResult = append(startProfilingResult, result)
	}

	// Create JSON result and send it to the client
	startProfilingResultInBytes, err := json.Marshal(startProfilingResult)
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	writeSuccessResponseJSON(w, []byte(startProfilingResultInBytes))
}

// dummyFileInfo represents a dummy representation of a profile data file
// present only in memory, it helps to generate the zip stream.
type dummyFileInfo struct {
	name    string
	size    int64
	mode    os.FileMode
	modTime time.Time
	isDir   bool
	sys     interface{}
}

func (f dummyFileInfo) Name() string       { return f.name }
func (f dummyFileInfo) Size() int64        { return f.size }
func (f dummyFileInfo) Mode() os.FileMode  { return f.mode }
func (f dummyFileInfo) ModTime() time.Time { return f.modTime }
func (f dummyFileInfo) IsDir() bool        { return f.isDir }
func (f dummyFileInfo) Sys() interface{}   { return f.sys }

// DownloadProfilingHandler - POST /minio/admin/v1/profiling/download
// ----------
// Download profiling information of all nodes in a zip format
func (a adminAPIHandlers) DownloadProfilingHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "DownloadProfiling")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	if !globalNotificationSys.DownloadProfilingData(ctx, w) {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrAdminProfilerNotEnabled), r.URL)
		return
	}
}

type healInitParams struct {
	bucket, objPrefix     string
	hs                    madmin.HealOpts
	clientToken           string
	forceStart, forceStop bool
}

// extractHealInitParams - Validates params for heal init API.
func extractHealInitParams(vars map[string]string, qParms url.Values, r io.Reader) (hip healInitParams, err APIErrorCode) {
	hip.bucket = vars[string(mgmtBucket)]
	hip.objPrefix = vars[string(mgmtPrefix)]

	if hip.bucket == "" {
		if hip.objPrefix != "" {
			// Bucket is required if object-prefix is given
			err = ErrHealMissingBucket
			return
		}
	} else if isReservedOrInvalidBucket(hip.bucket, false) {
		err = ErrInvalidBucketName
		return
	}

	// empty prefix is valid.
	if !IsValidObjectPrefix(hip.objPrefix) {
		err = ErrInvalidObjectName
		return
	}

	if len(qParms[string(mgmtClientToken)]) > 0 {
		hip.clientToken = qParms[string(mgmtClientToken)][0]
	}
	if _, ok := qParms[string(mgmtForceStart)]; ok {
		hip.forceStart = true
	}
	if _, ok := qParms[string(mgmtForceStop)]; ok {
		hip.forceStop = true
	}

	// Invalid request conditions:
	//
	//   Cannot have both forceStart and forceStop in the same
	//   request; If clientToken is provided, request can only be
	//   to continue receiving logs, so it cannot be start or
	//   stop;
	if (hip.forceStart && hip.forceStop) ||
		(hip.clientToken != "" && (hip.forceStart || hip.forceStop)) {
		err = ErrInvalidRequest
		return
	}

	// ignore body if clientToken is provided
	if hip.clientToken == "" {
		jerr := json.NewDecoder(r).Decode(&hip.hs)
		if jerr != nil {
			logger.LogIf(context.Background(), jerr)
			err = ErrRequestBodyParse
			return
		}
	}

	err = ErrNone
	return
}

// HealHandler - POST /minio/admin/v1/heal/
// -----------
// Start heal processing and return heal status items.
//
// On a successful heal sequence start, a unique client token is
// returned. Subsequent requests to this endpoint providing the client
// token will receive heal status records from the running heal
// sequence.
//
// If no client token is provided, and a heal sequence is in progress
// an error is returned with information about the running heal
// sequence. However, if the force-start flag is provided, the server
// aborts the running heal sequence and starts a new one.
func (a adminAPIHandlers) HealHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "Heal")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	// Check if this setup has an erasure coded backend.
	if !globalIsXL {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrHealNotImplemented), r.URL)
		return
	}

	hip, errCode := extractHealInitParams(mux.Vars(r), r.URL.Query(), r.Body)
	if errCode != ErrNone {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(errCode), r.URL)
		return
	}

	type healResp struct {
		respBytes []byte
		apiErr    APIError
		errBody   string
	}

	// Define a closure to start sending whitespace to client
	// after 10s unless a response item comes in
	keepConnLive := func(w http.ResponseWriter, respCh chan healResp) {
		ticker := time.NewTicker(time.Second * 10)
		defer ticker.Stop()
		started := false
	forLoop:
		for {
			select {
			case <-ticker.C:
				if !started {
					// Start writing response to client
					started = true
					setCommonHeaders(w)
					w.Header().Set(xhttp.ContentType, "text/event-stream")
					// Set 200 OK status
					w.WriteHeader(200)
				}
				// Send whitespace and keep connection open
				w.Write([]byte(" "))
				w.(http.Flusher).Flush()
			case hr := <-respCh:
				switch hr.apiErr {
				case noError:
					if started {
						w.Write(hr.respBytes)
						w.(http.Flusher).Flush()
					} else {
						writeSuccessResponseJSON(w, hr.respBytes)
					}
				default:
					var errorRespJSON []byte
					if hr.errBody == "" {
						errorRespJSON = encodeResponseJSON(getAPIErrorResponse(ctx, hr.apiErr,
							r.URL.Path, w.Header().Get(xhttp.AmzRequestID),
							globalDeploymentID))
					} else {
						errorRespJSON = encodeResponseJSON(APIErrorResponse{
							Code:      hr.apiErr.Code,
							Message:   hr.errBody,
							Resource:  r.URL.Path,
							RequestID: w.Header().Get(xhttp.AmzRequestID),
							HostID:    globalDeploymentID,
						})
					}
					if !started {
						setCommonHeaders(w)
						w.Header().Set(xhttp.ContentType, string(mimeJSON))
						w.WriteHeader(hr.apiErr.HTTPStatusCode)
					}
					w.Write(errorRespJSON)
					w.(http.Flusher).Flush()
				}
				break forLoop
			}
		}
	}

	// find number of disks in the setup
	info := objectAPI.StorageInfo(ctx)
	numDisks := info.Backend.OfflineDisks + info.Backend.OnlineDisks

	healPath := pathJoin(hip.bucket, hip.objPrefix)
	if hip.clientToken == "" && !hip.forceStart && !hip.forceStop {
		nh, exists := globalAllHealState.getHealSequence(healPath)
		if exists && !nh.hasEnded() && len(nh.currentStatus.Items) > 0 {
			b, err := json.Marshal(madmin.HealStartSuccess{
				ClientToken:   nh.clientToken,
				ClientAddress: nh.clientAddress,
				StartTime:     nh.startTime,
			})
			if err != nil {
				writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
				return
			}
			// Client token not specified but a heal sequence exists on a path,
			// Send the token back to client.
			writeSuccessResponseJSON(w, b)
			return
		}
	}

	if hip.clientToken != "" && !hip.forceStart && !hip.forceStop {
		// Since clientToken is given, fetch heal status from running
		// heal sequence.
		respBytes, errCode := globalAllHealState.PopHealStatusJSON(
			healPath, hip.clientToken)
		if errCode != ErrNone {
			writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(errCode), r.URL)
		} else {
			writeSuccessResponseJSON(w, respBytes)
		}
		return
	}

	respCh := make(chan healResp)
	switch {
	case hip.forceStop:
		go func() {
			respBytes, apiErr := globalAllHealState.stopHealSequence(healPath)
			hr := healResp{respBytes: respBytes, apiErr: apiErr}
			respCh <- hr
		}()
	case hip.clientToken == "":
		nh := newHealSequence(hip.bucket, hip.objPrefix, handlers.GetSourceIP(r), numDisks, hip.hs, hip.forceStart)
		go func() {
			respBytes, apiErr, errMsg := globalAllHealState.LaunchNewHealSequence(nh)
			hr := healResp{respBytes, apiErr, errMsg}
			respCh <- hr
		}()
	}

	// Due to the force-starting functionality, the Launch
	// call above can take a long time - to keep the
	// connection alive, we start sending whitespace
	keepConnLive(w, respCh)
}

func (a adminAPIHandlers) BackgroundHealStatusHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "HealBackgroundStatus")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	// Check if this setup has an erasure coded backend.
	if !globalIsXL {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrHealNotImplemented), r.URL)
		return
	}

	var bgHealStates []madmin.BgHealState

	// Get local heal status first
	bgHealStates = append(bgHealStates, getLocalBackgroundHealStatus())

	if globalIsDistXL {
		// Get heal status from other peers
		peersHealStates := globalNotificationSys.BackgroundHealStatus()
		bgHealStates = append(bgHealStates, peersHealStates...)
	}

	// Aggregate healing result
	var aggregatedHealStateResult = madmin.BgHealState{}
	for _, state := range bgHealStates {
		aggregatedHealStateResult.ScannedItemsCount += state.ScannedItemsCount
		if aggregatedHealStateResult.LastHealActivity.Before(state.LastHealActivity) {
			aggregatedHealStateResult.LastHealActivity = state.LastHealActivity
		}

	}

	if err := json.NewEncoder(w).Encode(aggregatedHealStateResult); err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	w.(http.Flusher).Flush()
}

// GetConfigHandler - GET /minio/admin/v1/config
// Get config.json of this minio setup.
func (a adminAPIHandlers) GetConfigHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "GetConfigHandler")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	config, err := readServerConfig(ctx, objectAPI)
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	configData, err := json.MarshalIndent(config, "", "\t")
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	password := config.GetCredential().SecretKey
	econfigData, err := madmin.EncryptData(password, configData)
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	writeSuccessResponseJSON(w, econfigData)
}

func validateAdminReq(ctx context.Context, w http.ResponseWriter, r *http.Request) ObjectLayer {
	// Get current object layer instance.
	objectAPI := newObjectLayerFn()
	if objectAPI == nil || globalNotificationSys == nil || globalIAMSys == nil {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrServerNotInitialized), r.URL)
		return nil
	}

	// Validate request signature.
	adminAPIErr := checkAdminRequestAuthType(ctx, r, "")
	if adminAPIErr != ErrNone {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(adminAPIErr), r.URL)
		return nil
	}

	return objectAPI
}

// AdminError - is a generic error for all admin APIs.
type AdminError struct {
	Code       string
	Message    string
	StatusCode int
}

func (ae AdminError) Error() string {
	return ae.Message
}

// Admin API errors
const (
	AdminUpdateUnexpectedFailure = "XMinioAdminUpdateUnexpectedFailure"
	AdminUpdateURLNotReachable   = "XMinioAdminUpdateURLNotReachable"
	AdminUpdateApplyFailure      = "XMinioAdminUpdateApplyFailure"
)

// toAdminAPIErrCode - converts errXLWriteQuorum error to admin API
// specific error.
func toAdminAPIErrCode(ctx context.Context, err error) APIErrorCode {
	switch err {
	case errXLWriteQuorum:
		return ErrAdminConfigNoQuorum
	default:
		return toAPIErrorCode(ctx, err)
	}
}

func toAdminAPIErr(ctx context.Context, err error) APIError {
	if err == nil {
		return noError
	}
	apiErr := errorCodes.ToAPIErr(toAdminAPIErrCode(ctx, err))
	if apiErr.Code == "InternalError" {
		switch e := err.(type) {
		case AdminError:
			apiErr = APIError{
				Code:           e.Code,
				Description:    e.Message,
				HTTPStatusCode: e.StatusCode,
			}
		}
	}
	return apiErr
}

// RemoveUser - DELETE /minio/admin/v1/remove-user?accessKey=<access_key>
func (a adminAPIHandlers) RemoveUser(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "RemoveUser")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	// Deny if WORM is enabled
	if globalWORMEnabled {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrMethodNotAllowed), r.URL)
		return
	}

	vars := mux.Vars(r)
	accessKey := vars["accessKey"]

	if err := globalIAMSys.DeleteUser(accessKey); err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	// Notify all other MinIO peers to delete user.
	for _, nerr := range globalNotificationSys.DeleteUser(accessKey) {
		if nerr.Err != nil {
			logger.GetReqInfo(ctx).SetTags("peerAddress", nerr.Host.String())
			logger.LogIf(ctx, nerr.Err)
		}
	}
}

// ListUsers - GET /minio/admin/v1/list-users
func (a adminAPIHandlers) ListUsers(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "ListUsers")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	allCredentials, err := globalIAMSys.ListUsers()
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	data, err := json.Marshal(allCredentials)
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	password := globalServerConfig.GetCredential().SecretKey
	econfigData, err := madmin.EncryptData(password, data)
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	writeSuccessResponseJSON(w, econfigData)
}

// GetUserInfo - GET /minio/admin/v1/user-info
func (a adminAPIHandlers) GetUserInfo(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "GetUserInfo")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	vars := mux.Vars(r)
	name := vars["accessKey"]

	userInfo, err := globalIAMSys.GetUserInfo(name)
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	data, err := json.Marshal(userInfo)
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	writeSuccessResponseJSON(w, data)
}

// UpdateGroupMembers - PUT /minio/admin/v1/update-group-members
func (a adminAPIHandlers) UpdateGroupMembers(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "UpdateGroupMembers")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	defer r.Body.Close()
	data, err := ioutil.ReadAll(r.Body)
	if err != nil {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrInvalidRequest), r.URL)
		return
	}

	var updReq madmin.GroupAddRemove
	err = json.Unmarshal(data, &updReq)
	if err != nil {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrInvalidRequest), r.URL)
		return
	}

	if updReq.IsRemove {
		err = globalIAMSys.RemoveUsersFromGroup(updReq.Group, updReq.Members)
	} else {
		err = globalIAMSys.AddUsersToGroup(updReq.Group, updReq.Members)
	}

	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	// Notify all other MinIO peers to load group.
	for _, nerr := range globalNotificationSys.LoadGroup(updReq.Group) {
		if nerr.Err != nil {
			logger.GetReqInfo(ctx).SetTags("peerAddress", nerr.Host.String())
			logger.LogIf(ctx, nerr.Err)
		}
	}
}

// GetGroup - /minio/admin/v1/group?group=mygroup1
func (a adminAPIHandlers) GetGroup(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "GetGroup")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	vars := mux.Vars(r)
	group := vars["group"]

	gdesc, err := globalIAMSys.GetGroupDescription(group)
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	body, err := json.Marshal(gdesc)
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	writeSuccessResponseJSON(w, body)
}

// ListGroups - GET /minio/admin/v1/groups
func (a adminAPIHandlers) ListGroups(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "ListGroups")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	groups, err := globalIAMSys.ListGroups()
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	body, err := json.Marshal(groups)
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	writeSuccessResponseJSON(w, body)
}

// SetGroupStatus - PUT /minio/admin/v1/set-group-status?group=mygroup1&status=enabled
func (a adminAPIHandlers) SetGroupStatus(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "SetGroupStatus")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	vars := mux.Vars(r)
	group := vars["group"]
	status := vars["status"]

	var err error
	if status == statusEnabled {
		err = globalIAMSys.SetGroupStatus(group, true)
	} else if status == statusDisabled {
		err = globalIAMSys.SetGroupStatus(group, false)
	} else {
		err = errInvalidArgument
	}
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	// Notify all other MinIO peers to reload user.
	for _, nerr := range globalNotificationSys.LoadGroup(group) {
		if nerr.Err != nil {
			logger.GetReqInfo(ctx).SetTags("peerAddress", nerr.Host.String())
			logger.LogIf(ctx, nerr.Err)
		}
	}
}

// SetUserStatus - PUT /minio/admin/v1/set-user-status?accessKey=<access_key>&status=[enabled|disabled]
func (a adminAPIHandlers) SetUserStatus(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "SetUserStatus")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	// Deny if WORM is enabled
	if globalWORMEnabled {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrMethodNotAllowed), r.URL)
		return
	}

	vars := mux.Vars(r)
	accessKey := vars["accessKey"]
	status := vars["status"]

	// Custom IAM policies not allowed for admin user.
	if accessKey == globalServerConfig.GetCredential().AccessKey {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrInvalidRequest), r.URL)
		return
	}

	if err := globalIAMSys.SetUserStatus(accessKey, madmin.AccountStatus(status)); err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	// Notify all other MinIO peers to reload user.
	for _, nerr := range globalNotificationSys.LoadUser(accessKey, false) {
		if nerr.Err != nil {
			logger.GetReqInfo(ctx).SetTags("peerAddress", nerr.Host.String())
			logger.LogIf(ctx, nerr.Err)
		}
	}
}

// AddUser - PUT /minio/admin/v1/add-user?accessKey=<access_key>
func (a adminAPIHandlers) AddUser(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "AddUser")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	// Deny if WORM is enabled
	if globalWORMEnabled {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrMethodNotAllowed), r.URL)
		return
	}

	vars := mux.Vars(r)
	accessKey := vars["accessKey"]

	// Custom IAM policies not allowed for admin user.
	if accessKey == globalServerConfig.GetCredential().AccessKey {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrAddUserInvalidArgument), r.URL)
		return
	}

	if r.ContentLength > maxEConfigJSONSize || r.ContentLength == -1 {
		// More than maxConfigSize bytes were available
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrAdminConfigTooLarge), r.URL)
		return
	}

	password := globalServerConfig.GetCredential().SecretKey
	configBytes, err := madmin.DecryptData(password, io.LimitReader(r.Body, r.ContentLength))
	if err != nil {
		logger.LogIf(ctx, err)
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrAdminConfigBadJSON), r.URL)
		return
	}

	var uinfo madmin.UserInfo
	if err = json.Unmarshal(configBytes, &uinfo); err != nil {
		logger.LogIf(ctx, err)
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrAdminConfigBadJSON), r.URL)
		return
	}

	if err = globalIAMSys.SetUser(accessKey, uinfo); err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	// Notify all other Minio peers to reload user
	for _, nerr := range globalNotificationSys.LoadUser(accessKey, false) {
		if nerr.Err != nil {
			logger.GetReqInfo(ctx).SetTags("peerAddress", nerr.Host.String())
			logger.LogIf(ctx, nerr.Err)
		}
	}
}

// InfoCannedPolicy - GET /minio/admin/v1/info-canned-policy?name={policyName}
func (a adminAPIHandlers) InfoCannedPolicy(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "InfoCannedPolicy")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	data, err := globalIAMSys.InfoPolicy(mux.Vars(r)["name"])
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	w.Write(data)
	w.(http.Flusher).Flush()
}

// ListCannedPolicies - GET /minio/admin/v1/list-canned-policies
func (a adminAPIHandlers) ListCannedPolicies(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "ListCannedPolicies")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	policies, err := globalIAMSys.ListPolicies()
	if err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	if err = json.NewEncoder(w).Encode(policies); err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	w.(http.Flusher).Flush()
}

// RemoveCannedPolicy - DELETE /minio/admin/v1/remove-canned-policy?name=<policy_name>
func (a adminAPIHandlers) RemoveCannedPolicy(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "RemoveCannedPolicy")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	vars := mux.Vars(r)
	policyName := vars["name"]

	// Deny if WORM is enabled
	if globalWORMEnabled {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrMethodNotAllowed), r.URL)
		return
	}

	if err := globalIAMSys.DeletePolicy(policyName); err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	// Notify all other MinIO peers to delete policy
	for _, nerr := range globalNotificationSys.DeletePolicy(policyName) {
		if nerr.Err != nil {
			logger.GetReqInfo(ctx).SetTags("peerAddress", nerr.Host.String())
			logger.LogIf(ctx, nerr.Err)
		}
	}
}

// AddCannedPolicy - PUT /minio/admin/v1/add-canned-policy?name=<policy_name>
func (a adminAPIHandlers) AddCannedPolicy(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "AddCannedPolicy")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	vars := mux.Vars(r)
	policyName := vars["name"]

	// Deny if WORM is enabled
	if globalWORMEnabled {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrMethodNotAllowed), r.URL)
		return
	}

	// Error out if Content-Length is missing.
	if r.ContentLength <= 0 {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrMissingContentLength), r.URL)
		return
	}

	// Error out if Content-Length is beyond allowed size.
	if r.ContentLength > maxBucketPolicySize {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrEntityTooLarge), r.URL)
		return
	}

	iamPolicy, err := iampolicy.ParseConfig(io.LimitReader(r.Body, r.ContentLength))
	if err != nil {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrMalformedPolicy), r.URL)
		return
	}

	// Version in policy must not be empty
	if iamPolicy.Version == "" {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrMalformedPolicy), r.URL)
		return
	}

	if err = globalIAMSys.SetPolicy(policyName, *iamPolicy); err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	// Notify all other MinIO peers to reload policy
	for _, nerr := range globalNotificationSys.LoadPolicy(policyName) {
		if nerr.Err != nil {
			logger.GetReqInfo(ctx).SetTags("peerAddress", nerr.Host.String())
			logger.LogIf(ctx, nerr.Err)
		}
	}
}

// SetPolicyForUserOrGroup - PUT /minio/admin/v1/set-policy?policy=xxx&user-or-group=?[&is-group]
func (a adminAPIHandlers) SetPolicyForUserOrGroup(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "SetPolicyForUserOrGroup")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	vars := mux.Vars(r)
	policyName := vars["policyName"]
	entityName := vars["userOrGroup"]
	isGroup := vars["isGroup"] == "true"

	// Deny if WORM is enabled
	if globalWORMEnabled {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrMethodNotAllowed), r.URL)
		return
	}

	if err := globalIAMSys.PolicyDBSet(entityName, policyName, isGroup); err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	// Notify all other MinIO peers to reload policy
	for _, nerr := range globalNotificationSys.LoadPolicyMapping(entityName, isGroup) {
		if nerr.Err != nil {
			logger.GetReqInfo(ctx).SetTags("peerAddress", nerr.Host.String())
			logger.LogIf(ctx, nerr.Err)
		}
	}
}

// SetConfigHandler - PUT /minio/admin/v1/config
func (a adminAPIHandlers) SetConfigHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "SetConfigHandler")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	// Deny if WORM is enabled
	if globalWORMEnabled {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrMethodNotAllowed), r.URL)
		return
	}

	if r.ContentLength > maxEConfigJSONSize || r.ContentLength == -1 {
		// More than maxConfigSize bytes were available
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrAdminConfigTooLarge), r.URL)
		return
	}

	password := globalServerConfig.GetCredential().SecretKey
	configBytes, err := madmin.DecryptData(password, io.LimitReader(r.Body, r.ContentLength))
	if err != nil {
		logger.LogIf(ctx, err)
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrAdminConfigBadJSON), r.URL)
		return
	}

	// Validate JSON provided in the request body: check the
	// client has not sent JSON objects with duplicate keys.
	if err = quick.CheckDuplicateKeys(string(configBytes)); err != nil {
		logger.LogIf(ctx, err)
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrAdminConfigBadJSON), r.URL)
		return
	}

	var config serverConfig
	if err = json.Unmarshal(configBytes, &config); err != nil {
		logger.LogIf(ctx, err)
		writeCustomErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrAdminConfigBadJSON), err.Error(), r.URL)
		return
	}

	// If credentials for the server are provided via environment,
	// then credentials in the provided configuration must match.
	if globalIsEnvCreds {
		if !globalServerConfig.GetCredential().Equal(config.Credential) {
			writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrAdminCredentialsMismatch), r.URL)
			return
		}
	}

	if err = config.Validate(); err != nil {
		writeCustomErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrAdminConfigBadJSON), err.Error(), r.URL)
		return
	}

	if err = config.TestNotificationTargets(); err != nil {
		writeCustomErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrAdminConfigBadJSON), err.Error(), r.URL)
		return
	}

	if err = saveServerConfig(ctx, objectAPI, &config); err != nil {
		writeErrorResponseJSON(ctx, w, toAdminAPIErr(ctx, err), r.URL)
		return
	}

	// Reply to the client before restarting minio server.
	writeSuccessResponseHeadersOnly(w)
}

// Returns true if the trace.Info should be traced,
// false if certain conditions are not met.
// - input entry is not of the type *trace.Info*
// - errOnly entries are to be traced, not status code 2xx, 3xx.
// - all entries to be traced, if not trace only S3 API requests.
func mustTrace(entry interface{}, trcAll, errOnly bool) bool {
	trcInfo, ok := entry.(trace.Info)
	if !ok {
		return false
	}
	trace := trcAll || !hasPrefix(trcInfo.ReqInfo.Path, minioReservedBucketPath+SlashSeparator)
	if errOnly {
		return trace && trcInfo.RespInfo.StatusCode >= http.StatusBadRequest
	}
	return trace
}

// TraceHandler - POST /minio/admin/v1/trace
// ----------
// The handler sends http trace to the connected HTTP client.
func (a adminAPIHandlers) TraceHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "HTTPTrace")
	trcAll := r.URL.Query().Get("all") == "true"
	trcErr := r.URL.Query().Get("err") == "true"

	// Validate request signature.
	adminAPIErr := checkAdminRequestAuthType(ctx, r, "")
	if adminAPIErr != ErrNone {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(adminAPIErr), r.URL)
		return
	}

	w.Header().Set(xhttp.ContentType, "text/event-stream")

	doneCh := make(chan struct{})
	defer close(doneCh)

	// Trace Publisher and peer-trace-client uses nonblocking send and hence does not wait for slow receivers.
	// Use buffered channel to take care of burst sends or slow w.Write()
	traceCh := make(chan interface{}, 4000)

	peers, err := getRestClients(getRemoteHosts(globalEndpoints))
	if err != nil {
		return
	}

	globalHTTPTrace.Subscribe(traceCh, doneCh, func(entry interface{}) bool {
		return mustTrace(entry, trcAll, trcErr)
	})

	for _, peer := range peers {
		peer.Trace(traceCh, doneCh, trcAll, trcErr)
	}

	keepAliveTicker := time.NewTicker(500 * time.Millisecond)
	defer keepAliveTicker.Stop()

	enc := json.NewEncoder(w)
	for {
		select {
		case entry := <-traceCh:
			if err := enc.Encode(entry); err != nil {
				return
			}
			w.(http.Flusher).Flush()
		case <-keepAliveTicker.C:
			if _, err := w.Write([]byte(" ")); err != nil {
				return
			}
			w.(http.Flusher).Flush()
		case <-GlobalServiceDoneCh:
			return
		}
	}
}

// The handler sends console logs to the connected HTTP client.
func (a adminAPIHandlers) ConsoleLogHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "ConsoleLog")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}
	node := r.URL.Query().Get("node")
	// limit buffered console entries if client requested it.
	limitStr := r.URL.Query().Get("limit")
	limitLines, err := strconv.Atoi(limitStr)
	if err != nil {
		limitLines = 10
	}
	// Avoid reusing tcp connection if read timeout is hit
	// This is needed to make r.Context().Done() work as
	// expected in case of read timeout
	w.Header().Add("Connection", "close")
	w.Header().Set(xhttp.ContentType, "text/event-stream")

	doneCh := make(chan struct{})
	defer close(doneCh)
	logCh := make(chan interface{}, 4000)

	remoteHosts := getRemoteHosts(globalEndpoints)
	peers, err := getRestClients(remoteHosts)
	if err != nil {
		return
	}

	globalConsoleSys.Subscribe(logCh, doneCh, node, limitLines, nil)

	for _, peer := range peers {
		if node == "" || strings.EqualFold(peer.host.Name, node) {
			peer.ConsoleLog(logCh, doneCh)
		}
	}

	enc := json.NewEncoder(w)

	keepAliveTicker := time.NewTicker(500 * time.Millisecond)
	defer keepAliveTicker.Stop()

	for {
		select {
		case entry := <-logCh:
			log := entry.(madmin.LogInfo)
			if log.SendLog(node) {
				if err := enc.Encode(log); err != nil {
					return
				}
				w.(http.Flusher).Flush()
			}
		case <-keepAliveTicker.C:
			if _, err := w.Write([]byte(" ")); err != nil {
				return
			}
			w.(http.Flusher).Flush()
		case <-GlobalServiceDoneCh:
			return
		}
	}
}

// KMSKeyStatusHandler - GET /minio/admin/v1/kms/key/status?key-id=<master-key-id>
func (a adminAPIHandlers) KMSKeyStatusHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "KMSKeyStatusHandler")

	objectAPI := validateAdminReq(ctx, w, r)
	if objectAPI == nil {
		return
	}

	if GlobalKMS == nil {
		writeErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrKMSNotConfigured), r.URL)
		return
	}

	keyID := r.URL.Query().Get("key-id")
	if keyID == "" {
		keyID = globalKMSKeyID
	}
	var response = madmin.KMSKeyStatus{
		KeyID: keyID,
	}

	kmsContext := crypto.Context{"MinIO admin API": "KMSKeyStatusHandler"} // Context for a test key operation
	// 1. Generate a new key using the KMS.
	key, sealedKey, err := GlobalKMS.GenerateKey(keyID, kmsContext)
	if err != nil {
		response.EncryptionErr = err.Error()
		resp, err := json.Marshal(response)
		if err != nil {
			writeCustomErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrInternalError), err.Error(), r.URL)
			return
		}
		writeSuccessResponseJSON(w, resp)
		return
	}

	// 2. Check whether we can update / re-wrap the sealed key.
	sealedKey, err = GlobalKMS.UpdateKey(keyID, sealedKey, kmsContext)
	if err != nil {
		response.UpdateErr = err.Error()
		resp, err := json.Marshal(response)
		if err != nil {
			writeCustomErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrInternalError), err.Error(), r.URL)
			return
		}
		writeSuccessResponseJSON(w, resp)
		return
	}

	// 3. Verify that we can indeed decrypt the (encrypted) key
	decryptedKey, err := GlobalKMS.UnsealKey(keyID, sealedKey, kmsContext)
	if err != nil {
		response.DecryptionErr = err.Error()
		resp, err := json.Marshal(response)
		if err != nil {
			writeCustomErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrInternalError), err.Error(), r.URL)
			return
		}
		writeSuccessResponseJSON(w, resp)
		return
	}

	// 4. Compare generated key with decrypted key
	if subtle.ConstantTimeCompare(key[:], decryptedKey[:]) != 1 {
		response.DecryptionErr = "The generated and the decrypted data key do not match"
		resp, err := json.Marshal(response)
		if err != nil {
			writeCustomErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrInternalError), err.Error(), r.URL)
			return
		}
		writeSuccessResponseJSON(w, resp)
		return
	}

	resp, err := json.Marshal(response)
	if err != nil {
		writeCustomErrorResponseJSON(ctx, w, errorCodes.ToAPIErr(ErrInternalError), err.Error(), r.URL)
		return
	}
	writeSuccessResponseJSON(w, resp)
}
