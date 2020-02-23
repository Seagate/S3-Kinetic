/*
 * MinIO Cloud Storage, (C) 2015, 2016, 2017, 2018 MinIO, Inc.
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
	"encoding/base64"
	"encoding/xml"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/url"
	"path"
	"path/filepath"
	"strings"

	"github.com/gorilla/mux"

	"github.com/minio/minio-go/v6/pkg/set"
	"github.com/minio/minio/cmd/crypto"
	xhttp "github.com/minio/minio/cmd/http"
	"github.com/minio/minio/cmd/logger"
	"github.com/minio/minio/pkg/dns"
	"github.com/minio/minio/pkg/event"
	"github.com/minio/minio/pkg/handlers"
	"github.com/minio/minio/pkg/hash"
	iampolicy "github.com/minio/minio/pkg/iam/policy"
	"github.com/minio/minio/pkg/policy"
	"github.com/minio/minio/pkg/sync/errgroup"
)

const (
	getBucketVersioningResponse       = `<VersioningConfiguration xmlns="http://s3.amazonaws.com/doc/2006-03-01/"/>`
	objectLockConfig                  = "object-lock.xml"
	bucketObjectLockEnabledConfigFile = "object-lock-enabled.json"
	bucketObjectLockEnabledConfig     = `{"x-amz-bucket-object-lock-enabled":true}`
)

// Check if there are buckets on server without corresponding entry in etcd backend and
// make entries. Here is the general flow
// - Range over all the available buckets
// - Check if a bucket has an entry in etcd backend
// -- If no, make an entry
// -- If yes, check if the IP of entry matches local IP. This means entry is for this instance.
// -- If IP of the entry doesn't match, this means entry is for another instance. Log an error to console.
func initFederatorBackend(buckets []BucketInfo, objLayer ObjectLayer) {
	//fmt.Println(" initFederatorBackend")
	if len(buckets) == 0 {
		return
	}

	// Get buckets in the DNS
	dnsBuckets, err := globalDNSConfig.List()
	if err != nil && err != dns.ErrNoEntriesFound {
		logger.LogIf(context.Background(), err)
		return
	}

	bucketSet := set.NewStringSet()

	// Add buckets that are not registered with the DNS
	g := errgroup.WithNErrs(len(buckets))
	for index := range buckets {
		bucketSet.Add(buckets[index].Name)
		index := index
		g.Go(func() error {
			r, gerr := globalDNSConfig.Get(buckets[index].Name)
			if gerr != nil {
				if gerr == dns.ErrNoEntriesFound {
					return globalDNSConfig.Put(buckets[index].Name)
				}
				return gerr
			}
			if globalDomainIPs.Intersection(set.CreateStringSet(getHostsSlice(r)...)).IsEmpty() {
				// There is already an entry for this bucket, with all IP addresses different. This indicates a bucket name collision. Log an error and continue.
				return fmt.Errorf("Unable to add bucket DNS entry for bucket %s, an entry exists for the same bucket. Use one of these IP addresses %v to access the bucket", buckets[index].Name, globalDomainIPs.ToSlice())
			}
			return nil
		}, index)
	}

	for _, err := range g.Wait() {
		if err != nil {
			logger.LogIf(context.Background(), err)
		}
	}

	g = errgroup.WithNErrs(len(dnsBuckets))
	// Remove buckets that are in DNS for this server, but aren't local
	for index := range dnsBuckets {
		index := index
		g.Go(func() error {
			// This is a local bucket that exists, so we can continue
			if bucketSet.Contains(dnsBuckets[index].Key) {
				return nil
			}

			// This is not for our server, so we can continue
			hostPort := net.JoinHostPort(dnsBuckets[index].Host, string(dnsBuckets[index].Port))
			if globalDomainIPs.Intersection(set.CreateStringSet(hostPort)).IsEmpty() {
				return nil
			}

			// We go to here, so we know the bucket no longer exists, but is registered in DNS to this server
			if err := globalDNSConfig.DeleteRecord(dnsBuckets[index]); err != nil {
				return fmt.Errorf("Failed to remove DNS entry for %s due to %v", dnsBuckets[index].Key, err)
			}

			return nil
		}, index)
	}

	for _, err := range g.Wait() {
		if err != nil {
			logger.LogIf(context.Background(), err)
		}
	}
	fmt.Println(" END initFederatorBackend")
}

// GetBucketLocationHandler - GET Bucket location.
// -------------------------
// This operation returns bucket location.
func (api objectAPIHandlers) GetBucketLocationHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "GetBucketLocation")

	defer logger.AuditLog(w, r, "GetBucketLocation", mustGetClaimsFromToken(r))

	vars := mux.Vars(r)
	bucket := vars["bucket"]

	objectAPI := api.ObjectAPI()
	if objectAPI == nil {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrServerNotInitialized), r.URL, guessIsBrowserReq(r))
		return
	}

	if s3Error := checkRequestAuthType(ctx, r, policy.GetBucketLocationAction, bucket, ""); s3Error != ErrNone {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(s3Error), r.URL, guessIsBrowserReq(r))
		return
	}

	getBucketInfo := objectAPI.GetBucketInfo

	if _, err := getBucketInfo(ctx, bucket); err != nil {
		writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
		return
	}

	// Generate response.
	encodedSuccessResponse := encodeResponse(LocationResponse{})
	// Get current region.
	region := globalServerRegion
	if region != globalMinioDefaultRegion {
		encodedSuccessResponse = encodeResponse(LocationResponse{
			Location: region,
		})
	}

	// Write success response.
	writeSuccessResponseXML(w, encodedSuccessResponse)
}

// ListMultipartUploadsHandler - GET Bucket (List Multipart uploads)
// -------------------------
// This operation lists in-progress multipart uploads. An in-progress
// multipart upload is a multipart upload that has been initiated,
// using the Initiate Multipart Upload request, but has not yet been
// completed or aborted. This operation returns at most 1,000 multipart
// uploads in the response.
//
func (api objectAPIHandlers) ListMultipartUploadsHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "ListMultipartUploads")

	defer logger.AuditLog(w, r, "ListMultipartUploads", mustGetClaimsFromToken(r))

	vars := mux.Vars(r)
	bucket := vars["bucket"]

	objectAPI := api.ObjectAPI()
	if objectAPI == nil {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrServerNotInitialized), r.URL, guessIsBrowserReq(r))
		return
	}

	if s3Error := checkRequestAuthType(ctx, r, policy.ListBucketMultipartUploadsAction, bucket, ""); s3Error != ErrNone {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(s3Error), r.URL, guessIsBrowserReq(r))
		return
	}

	prefix, keyMarker, uploadIDMarker, delimiter, maxUploads, encodingType, errCode := getBucketMultipartResources(r.URL.Query())
	if errCode != ErrNone {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(errCode), r.URL, guessIsBrowserReq(r))
		return
	}

	if maxUploads < 0 {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrInvalidMaxUploads), r.URL, guessIsBrowserReq(r))
		return
	}

	if keyMarker != "" {
		// Marker not common with prefix is not implemented.
		if !hasPrefix(keyMarker, prefix) {
			writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrNotImplemented), r.URL, guessIsBrowserReq(r))
			return
		}
	}

	listMultipartsInfo, err := objectAPI.ListMultipartUploads(ctx, bucket, prefix, keyMarker, uploadIDMarker, delimiter, maxUploads)
	if err != nil {
		writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
		return
	}
	// generate response
	response := generateListMultipartUploadsResponse(bucket, listMultipartsInfo, encodingType)
	encodedSuccessResponse := encodeResponse(response)

	// write success response.
	writeSuccessResponseXML(w, encodedSuccessResponse)
}

// ListBucketsHandler - GET Service.
// -----------
// This implementation of the GET operation returns a list of all buckets
// owned by the authenticated sender of the request.
func (api objectAPIHandlers) ListBucketsHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "ListBuckets")

	defer logger.AuditLog(w, r, "ListBuckets", mustGetClaimsFromToken(r))

	objectAPI := api.ObjectAPI()
	if objectAPI == nil {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrServerNotInitialized), r.URL, guessIsBrowserReq(r))
		return
	}

	listBuckets := objectAPI.ListBuckets

	accessKey, owner, s3Error := checkRequestAuthTypeToAccessKey(ctx, r, policy.ListAllMyBucketsAction, "", "")
	if s3Error != ErrNone {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(s3Error), r.URL, guessIsBrowserReq(r))
		return
	}

	// If etcd, dns federation configured list buckets from etcd.
	var bucketsInfo []BucketInfo
	if globalDNSConfig != nil {
		dnsBuckets, err := globalDNSConfig.List()
		if err != nil && err != dns.ErrNoEntriesFound {
			writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
			return
		}
		bucketSet := set.NewStringSet()
		for _, dnsRecord := range dnsBuckets {
			if bucketSet.Contains(dnsRecord.Key) {
				continue
			}
			bucketsInfo = append(bucketsInfo, BucketInfo{
				Name:    dnsRecord.Key,
				Created: dnsRecord.CreationDate,
			})
			bucketSet.Add(dnsRecord.Key)
		}
	} else {
		// Invoke the list buckets.
		var err error
		bucketsInfo, err = listBuckets(ctx)
		if err != nil {
			writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
			return
		}
	}

	// Set prefix value for "s3:prefix" policy conditionals.
	r.Header.Set("prefix", "")

	// Set delimiter value for "s3:delimiter" policy conditionals.
	r.Header.Set("delimiter", SlashSeparator)

	// err will be nil here as we already called this function
	// earlier in this request.
	claims, _ := getClaimsFromToken(r)
	var newBucketsInfo []BucketInfo
	for _, bucketInfo := range bucketsInfo {
		if globalIAMSys.IsAllowed(iampolicy.Args{
			AccountName:     accessKey,
			Action:          iampolicy.ListBucketAction,
			BucketName:      bucketInfo.Name,
			ConditionValues: getConditionValues(r, "", accessKey, claims),
			IsOwner:         owner,
			ObjectName:      "",
			Claims:          claims,
		}) {
			newBucketsInfo = append(newBucketsInfo, bucketInfo)
		}
	}

	// Generate response.
	response := generateListBucketsResponse(newBucketsInfo)
	encodedSuccessResponse := encodeResponse(response)

	// Write response.
	writeSuccessResponseXML(w, encodedSuccessResponse)
}

// DeleteMultipleObjectsHandler - deletes multiple objects.
func (api objectAPIHandlers) DeleteMultipleObjectsHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "DeleteMultipleObjects")

	defer logger.AuditLog(w, r, "DeleteMultipleObjects", mustGetClaimsFromToken(r))

	vars := mux.Vars(r)
	bucket := vars["bucket"]

	objectAPI := api.ObjectAPI()
	if objectAPI == nil {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrServerNotInitialized), r.URL, guessIsBrowserReq(r))
		return
	}

	// Content-Length is required and should be non-zero
	// http://docs.aws.amazon.com/AmazonS3/latest/API/multiobjectdeleteapi.html
	if r.ContentLength <= 0 {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrMissingContentLength), r.URL, guessIsBrowserReq(r))
		return
	}

	// Content-Md5 is requied should be set
	// http://docs.aws.amazon.com/AmazonS3/latest/API/multiobjectdeleteapi.html
	if _, ok := r.Header["Content-Md5"]; !ok {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrMissingContentMD5), r.URL, guessIsBrowserReq(r))
		return
	}

	// Allocate incoming content length bytes.
	var deleteXMLBytes []byte
	const maxBodySize = 2 * 1000 * 1024 // The max. XML contains 1000 object names (each at most 1024 bytes long) + XML overhead
	if r.ContentLength > maxBodySize {  // Only allocated memory for at most 1000 objects
		deleteXMLBytes = make([]byte, maxBodySize)
	} else {
		deleteXMLBytes = make([]byte, r.ContentLength)
	}

	// Read incoming body XML bytes.
	if _, err := io.ReadFull(r.Body, deleteXMLBytes); err != nil {
		logger.LogIf(ctx, err, logger.Application)
		writeErrorResponse(ctx, w, toAdminAPIErr(ctx, err), r.URL, guessIsBrowserReq(r))
		return
	}

	// Unmarshal list of keys to be deleted.
	deleteObjects := &DeleteObjectsRequest{}
	if err := xml.Unmarshal(deleteXMLBytes, deleteObjects); err != nil {
		logger.LogIf(ctx, err, logger.Application)
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrMalformedXML), r.URL, guessIsBrowserReq(r))
		return
	}

	deleteObjectsFn := objectAPI.DeleteObjects
	if api.CacheAPI() != nil {
		deleteObjectsFn = api.CacheAPI().DeleteObjects
	}

	var objectsToDelete = map[string]int{}
	getObjectInfoFn := objectAPI.GetObjectInfo
	if api.CacheAPI() != nil {
		getObjectInfoFn = api.CacheAPI().GetObjectInfo
	}

	var dErrs = make([]APIErrorCode, len(deleteObjects.Objects))

	for index, object := range deleteObjects.Objects {
		if dErrs[index] = checkRequestAuthType(ctx, r, policy.DeleteObjectAction, bucket, object.ObjectName); dErrs[index] != ErrNone {
			if dErrs[index] == ErrSignatureDoesNotMatch || dErrs[index] == ErrInvalidAccessKeyID {
				writeErrorResponse(ctx, w, errorCodes.ToAPIErr(dErrs[index]), r.URL, guessIsBrowserReq(r))
				return
			}
			continue
		}
		govBypassPerms := checkRequestAuthType(ctx, r, policy.BypassGovernanceRetentionAction, bucket, object.ObjectName)
		if _, err := checkGovernanceBypassAllowed(ctx, r, bucket, object.ObjectName, getObjectInfoFn, govBypassPerms); err != ErrNone {
			dErrs[index] = err
			continue
		}
		// Avoid duplicate objects, we use map to filter them out.
		if _, ok := objectsToDelete[object.ObjectName]; !ok {
			objectsToDelete[object.ObjectName] = index
		}
	}

	toNames := func(input map[string]int) (output []string) {
		output = make([]string, len(input))
		idx := 0
		for name := range input {
			output[idx] = name
			idx++
		}
		return
	}

	deleteList := toNames(objectsToDelete)
	errs, err := deleteObjectsFn(ctx, bucket, deleteList)
	if err != nil {
		writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
		return
	}

	for i, objName := range deleteList {
		dIdx := objectsToDelete[objName]
		dErrs[dIdx] = toAPIErrorCode(ctx, errs[i])
	}

	// Collect deleted objects and errors if any.
	var deletedObjects []ObjectIdentifier
	var deleteErrors []DeleteError
	for index, errCode := range dErrs {
		object := deleteObjects.Objects[index]
		// Success deleted objects are collected separately.
		if errCode == ErrNone || errCode == ErrNoSuchKey {
			deletedObjects = append(deletedObjects, object)
			continue
		}
		apiErr := getAPIError(errCode)
		// Error during delete should be collected separately.
		deleteErrors = append(deleteErrors, DeleteError{
			Code:    apiErr.Code,
			Message: apiErr.Description,
			Key:     object.ObjectName,
		})
	}

	// Generate response
	response := generateMultiDeleteResponse(deleteObjects.Quiet, deletedObjects, deleteErrors)
	encodedSuccessResponse := encodeResponse(response)

	// Write success response.
	writeSuccessResponseXML(w, encodedSuccessResponse)

	// Notify deleted event for objects.
	for _, dobj := range deletedObjects {
		sendEvent(eventArgs{
			EventName:  event.ObjectRemovedDelete,
			BucketName: bucket,
			Object: ObjectInfo{
				Name: dobj.ObjectName,
			},
			ReqParams:    extractReqParams(r),
			RespElements: extractRespElements(w),
			UserAgent:    r.UserAgent(),
			Host:         handlers.GetSourceIP(r),
		})
	}
}

// PutBucketHandler - PUT Bucket
// ----------
// This implementation of the PUT operation creates a new bucket for authenticated request
func (api objectAPIHandlers) PutBucketHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "PutBucket")

	defer logger.AuditLog(w, r, "PutBucket", mustGetClaimsFromToken(r))

	objectAPI := api.ObjectAPI()
	if objectAPI == nil {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrServerNotInitialized), r.URL, guessIsBrowserReq(r))
		return
	}

	vars := mux.Vars(r)
	bucket := vars["bucket"]

	objectLockEnabled := false
	if _, found := r.Header[http.CanonicalHeaderKey("x-amz-bucket-object-lock-enabled")]; found {
		if r.Header.Get("x-amz-bucket-object-lock-enabled") != "true" {
			writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrInvalidRequest), r.URL, guessIsBrowserReq(r))
			return
		}

		objectLockEnabled = true
	}

	if s3Error := checkRequestAuthType(ctx, r, policy.CreateBucketAction, bucket, ""); s3Error != ErrNone {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(s3Error), r.URL, guessIsBrowserReq(r))
		return
	}

	// Parse incoming location constraint.
	location, s3Error := parseLocationConstraint(r)
	if s3Error != ErrNone {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(s3Error), r.URL, guessIsBrowserReq(r))
		return
	}

	// Validate if location sent by the client is valid, reject
	// requests which do not follow valid region requirements.
	if !isValidLocation(location) {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrInvalidRegion), r.URL, guessIsBrowserReq(r))
		return
	}

	if globalDNSConfig != nil {
		if _, err := globalDNSConfig.Get(bucket); err != nil {
			if err == dns.ErrNoEntriesFound {
				// Proceed to creating a bucket.
				if err = objectAPI.MakeBucketWithLocation(ctx, bucket, location); err != nil {
					writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
					return
				}

				if objectLockEnabled {
					configFile := path.Join(bucketConfigPrefix, bucket, bucketObjectLockEnabledConfigFile)
					if err = saveConfig(ctx, objectAPI, configFile, []byte(bucketObjectLockEnabledConfig)); err != nil {
						writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
						return
					}
				}

				if err = globalDNSConfig.Put(bucket); err != nil {
					objectAPI.DeleteBucket(ctx, bucket)
					writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
					return
				}

				// Make sure to add Location information here only for bucket
				w.Header().Set(xhttp.Location,
					getObjectLocation(r, globalDomainNames, bucket, ""))

				writeSuccessResponseHeadersOnly(w)
				return
			}
			writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
			return

		}
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrBucketAlreadyOwnedByYou), r.URL, guessIsBrowserReq(r))
		return
	}

	// Proceed to creating a bucket.
	err := objectAPI.MakeBucketWithLocation(ctx, bucket, location)
	if err != nil {
		writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
		return
	}

	if objectLockEnabled {
		configFile := path.Join(bucketConfigPrefix, bucket, bucketObjectLockEnabledConfigFile)
		if err = saveConfig(ctx, objectAPI, configFile, []byte(bucketObjectLockEnabledConfig)); err != nil {
			writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
			return
		}
		globalBucketObjectLockConfig.Set(bucket, Retention{})
	}

	// Make sure to add Location information here only for bucket
	w.Header().Set(xhttp.Location, path.Clean(r.URL.Path)) // Clean any trailing slashes.

	writeSuccessResponseHeadersOnly(w)
}

// PostPolicyBucketHandler - POST policy
// ----------
// This implementation of the POST operation handles object creation with a specified
// signature policy in multipart/form-data
func (api objectAPIHandlers) PostPolicyBucketHandler(w http.ResponseWriter, r *http.Request) {
	//fmt.Println("POST POLICY BUCKET HANDLER")
	ctx := newContext(r, w, "PostPolicyBucket")

	defer logger.AuditLog(w, r, "PostPolicyBucket", mustGetClaimsFromToken(r))

	objectAPI := api.ObjectAPI()
	if objectAPI == nil {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrServerNotInitialized), r.URL, guessIsBrowserReq(r))
		return
	}

	if crypto.S3KMS.IsRequested(r.Header) { // SSE-KMS is not supported
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrNotImplemented), r.URL, guessIsBrowserReq(r))
		return
	}
	if !api.EncryptionEnabled() && crypto.IsRequested(r.Header) {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrNotImplemented), r.URL, guessIsBrowserReq(r))
		return
	}

	bucket := mux.Vars(r)["bucket"]

	// To detect if the client has disconnected.
	r.Body = &detectDisconnect{r.Body, r.Context().Done()}

	// Require Content-Length to be set in the request
	size := r.ContentLength
	if size < 0 {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrMissingContentLength), r.URL, guessIsBrowserReq(r))
		return
	}
	resource, err := getResource(r.URL.Path, r.Host, globalDomainNames)
	if err != nil {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrInvalidRequest), r.URL, guessIsBrowserReq(r))
		return
	}
	// Make sure that the URL  does not contain object name.
	if bucket != filepath.Clean(resource[1:]) {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrMethodNotAllowed), r.URL, guessIsBrowserReq(r))
		return
	}

	// Here the parameter is the size of the form data that should
	// be loaded in memory, the remaining being put in temporary files.
	reader, err := r.MultipartReader()
	if err != nil {
		logger.LogIf(ctx, err)
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrMalformedPOSTRequest), r.URL, guessIsBrowserReq(r))
		return
	}

	// Read multipart data and save in memory and in the disk if needed
	form, err := reader.ReadForm(maxFormMemory)
	if err != nil {
		logger.LogIf(ctx, err, logger.Application)
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrMalformedPOSTRequest), r.URL, guessIsBrowserReq(r))
		return
	}

	// Remove all tmp files created during multipart upload
	defer form.RemoveAll()

	// Extract all form fields
	fileBody, fileName, fileSize, formValues, err := extractPostPolicyFormValues(ctx, form)
	if err != nil {
		logger.LogIf(ctx, err, logger.Application)
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrMalformedPOSTRequest), r.URL, guessIsBrowserReq(r))
		return
	}

	// Check if file is provided, error out otherwise.
	if fileBody == nil {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrPOSTFileRequired), r.URL, guessIsBrowserReq(r))
		return
	}

	// Close multipart file
	defer fileBody.Close()

	formValues.Set("Bucket", bucket)

	if fileName != "" && strings.Contains(formValues.Get("Key"), "${filename}") {
		// S3 feature to replace ${filename} found in Key form field
		// by the filename attribute passed in multipart
		formValues.Set("Key", strings.Replace(formValues.Get("Key"), "${filename}", fileName, -1))
	}
	object := formValues.Get("Key")

	successRedirect := formValues.Get("success_action_redirect")
	successStatus := formValues.Get("success_action_status")
	var redirectURL *url.URL
	if successRedirect != "" {
		redirectURL, err = url.Parse(successRedirect)
		if err != nil {
			writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrMalformedPOSTRequest), r.URL, guessIsBrowserReq(r))
			return
		}
	}

	// Verify policy signature.
	errCode := doesPolicySignatureMatch(formValues)
	if errCode != ErrNone {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(errCode), r.URL, guessIsBrowserReq(r))
		return
	}

	policyBytes, err := base64.StdEncoding.DecodeString(formValues.Get("Policy"))
	if err != nil {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrMalformedPOSTRequest), r.URL, guessIsBrowserReq(r))
		return
	}

	// Handle policy if it is set.
	if len(policyBytes) > 0 {

		postPolicyForm, err := parsePostPolicyForm(string(policyBytes))
		if err != nil {
			writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrPostPolicyConditionInvalidFormat), r.URL, guessIsBrowserReq(r))
			return
		}

		// Make sure formValues adhere to policy restrictions.
		if err = checkPostPolicy(formValues, postPolicyForm); err != nil {
			writeCustomErrorResponseXML(ctx, w, errorCodes.ToAPIErr(ErrAccessDenied), err.Error(), r.URL, guessIsBrowserReq(r))
			return
		}

		// Ensure that the object size is within expected range, also the file size
		// should not exceed the maximum single Put size (5 GiB)
		lengthRange := postPolicyForm.Conditions.ContentLengthRange
		if lengthRange.Valid {
			if fileSize < lengthRange.Min {
				writeErrorResponse(ctx, w, toAPIError(ctx, errDataTooSmall), r.URL, guessIsBrowserReq(r))
				return
			}

			if fileSize > lengthRange.Max || isMaxObjectSize(fileSize) {
				writeErrorResponse(ctx, w, toAPIError(ctx, errDataTooLarge), r.URL, guessIsBrowserReq(r))
				return
			}
		}
	}

	// Extract metadata to be saved from received Form.
	metadata := make(map[string]string)
	err = extractMetadataFromMap(ctx, formValues, metadata)
	if err != nil {
		writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
		return
	}

	hashReader, err := hash.NewReader(fileBody, fileSize, "", "", fileSize, globalCLIContext.StrictS3Compat)
	if err != nil {
		logger.LogIf(ctx, err)
		writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
		return
	}
	rawReader := hashReader
	pReader := NewPutObjReader(rawReader, nil, nil)
	var objectEncryptionKey []byte

	// This request header needs to be set prior to setting ObjectOptions
	if globalAutoEncryption && !crypto.SSEC.IsRequested(r.Header) {
		r.Header.Add(crypto.SSEHeader, crypto.SSEAlgorithmAES256)
	}
	// get gateway encryption options
	var opts ObjectOptions
	opts, err = putOpts(ctx, r, bucket, object, metadata)
	if err != nil {
		writeErrorResponseHeadersOnly(w, toAPIError(ctx, err))
		return
	}
	if objectAPI.IsEncryptionSupported() {
		if crypto.IsRequested(formValues) && !hasSuffix(object, SlashSeparator) { // handle SSE requests
			if crypto.SSECopy.IsRequested(r.Header) {
				writeErrorResponse(ctx, w, toAPIError(ctx, errInvalidEncryptionParameters), r.URL, guessIsBrowserReq(r))
				return
			}
			var reader io.Reader
			var key []byte
			if crypto.SSEC.IsRequested(formValues) {
				key, err = ParseSSECustomerHeader(formValues)
				if err != nil {
					writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
					return
				}
			}
			reader, objectEncryptionKey, err = newEncryptReader(hashReader, key, bucket, object, metadata, crypto.S3.IsRequested(formValues))
			if err != nil {
				writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
				return
			}
			info := ObjectInfo{Size: fileSize}
			// do not try to verify encrypted content
			hashReader, err = hash.NewReader(reader, info.EncryptedSize(), "", "", fileSize, globalCLIContext.StrictS3Compat)
			if err != nil {
				writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
				return
			}
			pReader = NewPutObjReader(rawReader, hashReader, objectEncryptionKey)
		}
	}
	objInfo, err := objectAPI.PutObject(ctx, bucket, object, pReader, opts)
	if err != nil {
		writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
		return
	}

	location := getObjectLocation(r, globalDomainNames, bucket, object)
	w.Header()[xhttp.ETag] = []string{`"` + objInfo.ETag + `"`}
	w.Header().Set(xhttp.Location, location)

	// Notify object created event.
	defer sendEvent(eventArgs{
		EventName:    event.ObjectCreatedPost,
		BucketName:   objInfo.Bucket,
		Object:       objInfo,
		ReqParams:    extractReqParams(r),
		RespElements: extractRespElements(w),
		UserAgent:    r.UserAgent(),
		Host:         handlers.GetSourceIP(r),
	})

	if successRedirect != "" {
		// Replace raw query params..
		redirectURL.RawQuery = getRedirectPostRawQuery(objInfo)
		writeRedirectSeeOther(w, redirectURL.String())
		return
	}

	// Decide what http response to send depending on success_action_status parameter
	switch successStatus {
	case "201":
		resp := encodeResponse(PostResponse{
			Bucket:   objInfo.Bucket,
			Key:      objInfo.Name,
			ETag:     `"` + objInfo.ETag + `"`,
			Location: location,
		})
		writeResponse(w, http.StatusCreated, resp, "application/xml")
	case "200":
		writeSuccessResponseHeadersOnly(w)
	default:
		writeSuccessNoContent(w)
	}
}

// HeadBucketHandler - HEAD Bucket
// ----------
// This operation is useful to determine if a bucket exists.
// The operation returns a 200 OK if the bucket exists and you
// have permission to access it. Otherwise, the operation might
// return responses such as 404 Not Found and 403 Forbidden.
func (api objectAPIHandlers) HeadBucketHandler(w http.ResponseWriter, r *http.Request) {
	//fmt.Println("HEAD BUCKET HANDLER")
	ctx := newContext(r, w, "HeadBucket")

	defer logger.AuditLog(w, r, "HeadBucket", mustGetClaimsFromToken(r))

	vars := mux.Vars(r)
	bucket := vars["bucket"]

	objectAPI := api.ObjectAPI()
	if objectAPI == nil {
		writeErrorResponseHeadersOnly(w, errorCodes.ToAPIErr(ErrServerNotInitialized))
		return
	}

	if s3Error := checkRequestAuthType(ctx, r, policy.ListBucketAction, bucket, ""); s3Error != ErrNone {
		writeErrorResponseHeadersOnly(w, errorCodes.ToAPIErr(s3Error))
		return
	}

	getBucketInfo := objectAPI.GetBucketInfo

	if _, err := getBucketInfo(ctx, bucket); err != nil {
		writeErrorResponseHeadersOnly(w, toAPIError(ctx, err))
		return
	}

	writeSuccessResponseHeadersOnly(w)
}

// DeleteBucketHandler - Delete bucket
func (api objectAPIHandlers) DeleteBucketHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "DeleteBucket")

	defer logger.AuditLog(w, r, "DeleteBucket", mustGetClaimsFromToken(r))

	vars := mux.Vars(r)
	bucket := vars["bucket"]

	objectAPI := api.ObjectAPI()
	if objectAPI == nil {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrServerNotInitialized), r.URL, guessIsBrowserReq(r))
		return
	}

	if s3Error := checkRequestAuthType(ctx, r, policy.DeleteBucketAction, bucket, ""); s3Error != ErrNone {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(s3Error), r.URL, guessIsBrowserReq(r))
		return
	}

	deleteBucket := objectAPI.DeleteBucket

	// Attempt to delete bucket.
	if err := deleteBucket(ctx, bucket); err != nil {
		writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
		return
	}

	if globalDNSConfig != nil {
		if err := globalDNSConfig.Delete(bucket); err != nil {
			// Deleting DNS entry failed, attempt to create the bucket again.
			objectAPI.MakeBucketWithLocation(ctx, bucket, "")
			writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
			return
		}
	}

	globalBucketObjectLockConfig.Delete(bucket)
	globalNotificationSys.RemoveNotification(bucket)
	globalPolicySys.Remove(bucket)
	globalNotificationSys.DeleteBucket(ctx, bucket)
	globalLifecycleSys.Remove(bucket)
	globalNotificationSys.RemoveBucketLifecycle(ctx, bucket)

	// Write success response.
	writeSuccessNoContent(w)
}

// PutBucketVersioningHandler - PUT Bucket Versioning.
// ----------
// No-op. Available for API compatibility.
func (api objectAPIHandlers) PutBucketVersioningHandler(w http.ResponseWriter, r *http.Request) {
	ctx := newContext(r, w, "PutBucketVersioning")

	defer logger.AuditLog(w, r, "PutBucketVersioning", mustGetClaimsFromToken(r))

	vars := mux.Vars(r)
	bucket := vars["bucket"]

	objectAPI := api.ObjectAPI()
	if objectAPI == nil {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrServerNotInitialized), r.URL, guessIsBrowserReq(r))
		return
	}

	getBucketInfo := objectAPI.GetBucketInfo
	if _, err := getBucketInfo(ctx, bucket); err != nil {
		writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
		return
	}

	// Write success response.
	writeSuccessResponseHeadersOnly(w)
}

// GetBucketVersioningHandler - GET Bucket Versioning.
// ----------
// No-op. Available for API compatibility.
func (api objectAPIHandlers) GetBucketVersioningHandler(w http.ResponseWriter, r *http.Request) {
	//fmt.Println("GET BUCKET VERSION HANDLER")
	ctx := newContext(r, w, "GetBucketVersioning")

	defer logger.AuditLog(w, r, "GetBucketVersioning", mustGetClaimsFromToken(r))

	vars := mux.Vars(r)
	bucket := vars["bucket"]

	objectAPI := api.ObjectAPI()
	if objectAPI == nil {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrServerNotInitialized), r.URL, guessIsBrowserReq(r))
		return
	}

	getBucketInfo := objectAPI.GetBucketInfo
	if _, err := getBucketInfo(ctx, bucket); err != nil {
		writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
		return
	}

	// Write success response.
	writeSuccessResponseXML(w, []byte(getBucketVersioningResponse))
}

// PutBucketObjectLockConfigHandler - PUT Bucket object lock configuration.
// ----------
// Places an Object Lock configuration on the specified bucket. The rule
// specified in the Object Lock configuration will be applied by default
// to every new object placed in the specified bucket.
func (api objectAPIHandlers) PutBucketObjectLockConfigHandler(w http.ResponseWriter, r *http.Request) {
	//fmt.Println("PIT BUCKET OBJECT LOCK CONFIG HANDLER")
	ctx := newContext(r, w, "PutBucketObjectLockConfig")

	defer logger.AuditLog(w, r, "PutBucketObjectLockConfig", mustGetClaimsFromToken(r))

	vars := mux.Vars(r)
	bucket := vars["bucket"]

	objectAPI := api.ObjectAPI()
	if objectAPI == nil {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrServerNotInitialized), r.URL, guessIsBrowserReq(r))
		return
	}

	// Deny if WORM is enabled
	if globalWORMEnabled {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrMethodNotAllowed), r.URL, guessIsBrowserReq(r))
		return
	}
	if s3Error := checkRequestAuthType(ctx, r, policy.PutBucketObjectLockConfigurationAction, bucket, ""); s3Error != ErrNone {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(s3Error), r.URL, guessIsBrowserReq(r))
		return
	}
	config, err := parseObjectLockConfig(r.Body)
	if err != nil {
		apiErr := errorCodes.ToAPIErr(ErrMalformedXML)
		apiErr.Description = err.Error()
		writeErrorResponse(ctx, w, apiErr, r.URL, guessIsBrowserReq(r))
		return
	}
	configFile := path.Join(bucketConfigPrefix, bucket, bucketObjectLockEnabledConfigFile)
	configData, err := readConfig(ctx, objectAPI, configFile)
	if err != nil {
		aerr := toAPIError(ctx, err)
		if err == errConfigNotFound {
			aerr = errorCodes.ToAPIErr(ErrMethodNotAllowed)
		}
		writeErrorResponse(ctx, w, aerr, r.URL, guessIsBrowserReq(r))
		return
	}

	if string(configData) != bucketObjectLockEnabledConfig {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrInternalError), r.URL, guessIsBrowserReq(r))
		return
	}
	data, err := xml.Marshal(config)
	if err != nil {
		writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
		return
	}
	configFile = path.Join(bucketConfigPrefix, bucket, objectLockConfig)
	if err = saveConfig(ctx, objectAPI, configFile, data); err != nil {
		writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
		return
	}
	if config.Rule != nil {
		retention := config.ToRetention()
		globalBucketObjectLockConfig.Set(bucket, retention)
		globalNotificationSys.PutBucketObjectLockConfig(ctx, bucket, retention)
	} else {
		globalBucketObjectLockConfig.Set(bucket, Retention{})
		globalBucketObjectLockConfig.Delete(bucket)
	}

	// Write success response.
	writeSuccessResponseHeadersOnly(w)
}

// GetBucketObjectLockConfigHandler - GET Bucket object lock configuration.
// ----------
// Gets the Object Lock configuration for a bucket. The rule specified in
// the Object Lock configuration will be applied by default to every new
// object placed in the specified bucket.
func (api objectAPIHandlers) GetBucketObjectLockConfigHandler(w http.ResponseWriter, r *http.Request) {
	//fmt.Println("GET BUCKET OBJECT LOCK CONFIG HANDLER")
	ctx := newContext(r, w, "GetBucketObjectLockConfig")

	defer logger.AuditLog(w, r, "GetBucketObjectLockConfig", mustGetClaimsFromToken(r))

	vars := mux.Vars(r)
	bucket := vars["bucket"]

	objectAPI := api.ObjectAPI()
	if objectAPI == nil {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrServerNotInitialized), r.URL, guessIsBrowserReq(r))
		return
	}
	// check if user has permissions to perform this operation
	if s3Error := checkRequestAuthType(ctx, r, policy.GetBucketObjectLockConfigurationAction, bucket, ""); s3Error != ErrNone {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(s3Error), r.URL, guessIsBrowserReq(r))
		return
	}
	configFile := path.Join(bucketConfigPrefix, bucket, bucketObjectLockEnabledConfigFile)
	configData, err := readConfig(ctx, objectAPI, configFile)
	if err != nil {
		var aerr APIError
		if err == errConfigNotFound {
			aerr = errorCodes.ToAPIErr(ErrMethodNotAllowed)
		} else {
			aerr = toAPIError(ctx, err)
		}
		writeErrorResponse(ctx, w, aerr, r.URL, guessIsBrowserReq(r))
		return
	}

	if string(configData) != bucketObjectLockEnabledConfig {
		writeErrorResponse(ctx, w, errorCodes.ToAPIErr(ErrInternalError), r.URL, guessIsBrowserReq(r))
		return
	}

	configFile = path.Join(bucketConfigPrefix, bucket, objectLockConfig)
	configData, err = readConfig(ctx, objectAPI, configFile)
	if err != nil {
		if err != errConfigNotFound {
			writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
			return
		}

		if configData, err = xml.Marshal(newObjectLockConfig()); err != nil {
			writeErrorResponse(ctx, w, toAPIError(ctx, err), r.URL, guessIsBrowserReq(r))
			return
		}
	}

	// Write success response.
	writeSuccessResponseXML(w, configData)
}
