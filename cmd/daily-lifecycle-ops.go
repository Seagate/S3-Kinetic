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

package cmd

import (
	"context"
	//"log"
	"time"
    "fmt"

	"github.com/minio/minio/cmd/logger"
	"github.com/minio/minio/pkg/bucket/lifecycle"
	"github.com/minio/minio/pkg/event"
	"github.com/minio/minio/common"
)

const (
	bgLifecycleInterval = 24 * time.Hour
	bgLifecycleTick     = time.Hour
)

type lifecycleOps struct {
	LastActivity time.Time
}

// Register to the daily objects listing
var globalLifecycleOps = &lifecycleOps{}

func getLocalBgLifecycleOpsStatus() BgLifecycleOpsStatus {
    defer common.KUntrace(common.KTrace("Enter"))
	return BgLifecycleOpsStatus{
		LastActivity: globalLifecycleOps.LastActivity,
	}
}

// initDailyLifecycle starts the routine that receives the daily
// listing of all objects and applies any matching bucket lifecycle
// rules.
func initDailyLifecycle() {
    defer common.KUntrace(common.KTrace("Enter"))
	go startDailyLifecycle()
}

func startDailyLifecycle() {
    defer common.KUntrace(common.KTrace("Enter"))
	var objAPI ObjectLayer
	var ctx = context.Background()
	//log.Println("START DAILY LIFECYCLE")
	// Wait until the object API is ready
	for {
		objAPI = newObjectLayerWithoutSafeModeFn()
		if objAPI == nil {
            common.KTrace("****** Object API is not ready.  Sleeping...")
			time.Sleep(time.Second)
			continue
		}
		break
	}

	// Calculate the time of the last lifecycle operation in all peers node of the cluster
	computeLastLifecycleActivity := func(status []BgOpsStatus) time.Time {
        defer common.KUntrace(common.KTrace("Enter"))
		//log.Println(" CALCULATE TIME OF LAST LIFECYCLE")
		var lastAct time.Time
		for _, st := range status {
			if st.LifecycleOps.LastActivity.After(lastAct) {
				lastAct = st.LifecycleOps.LastActivity
			}
		}
		return lastAct
	}

	for {
        common.KTrace("****** Wake up")
		// Check if we should perform lifecycle ops based on the last lifecycle activity, sleep one hour otherwise

		allLifecycleStatus := []BgOpsStatus{
			{LifecycleOps: getLocalBgLifecycleOpsStatus()},
		}
		if globalIsDistXL {
			allLifecycleStatus = append(allLifecycleStatus, globalNotificationSys.BackgroundOpsStatus()...)
		}
		lastAct := computeLastLifecycleActivity(allLifecycleStatus)
		if !lastAct.IsZero() && time.Since(lastAct) < bgLifecycleInterval {
            common.KTrace("****** There was action.  Sleeping...")
			time.Sleep(bgLifecycleTick)
		}
		//log.Println("PERFORM LIFECYCLE OP")
		// Perform one lifecycle operation
		err := lifecycleRound(ctx, objAPI)
                //log.Println("1. PERFORM LIFECYCLE OP", err)

		switch err.(type) {
		// Unable to hold a lock means there is another
		// instance doing the lifecycle round round
		case OperationTimedOut:
                        //log.Println("2.1 PERFORM LIFECYCLE OP")
            common.KTrace("****** OperationTimedOut: Sleeping...")
			time.Sleep(bgLifecycleTick)
		default:
			logger.LogIf(ctx, err)
                        //log.Println("2.2 PERFORM LIFECYCLE OP")
            common.KTrace("****** OperationTimedOut: Default Sleeping...")
			time.Sleep(time.Minute)
			continue
		}

	}
        //log.Println("3. PERFORM LIFECYCLE OP")

}

var lifecycleLockTimeout = newDynamicTimeout(60*time.Second, time.Second)

func lifecycleRound(ctx context.Context, objAPI ObjectLayer) error {
    defer common.KUntrace(common.KTrace("Enter"))
	// Lock to avoid concurrent lifecycle ops from other nodes
        //log.Println("LIFECYCLE ROUND")

	sweepLock := objAPI.NewNSLock(ctx, "system", "daily-lifecycle-ops")
	if err := sweepLock.GetLock(lifecycleLockTimeout); err != nil {
		return err
	}
	defer sweepLock.Unlock()
	//log.Println("1. LIFECYCLE ROUND")
	buckets, err := objAPI.ListBuckets(ctx)
	if err != nil {
		return err
	}

	for _, bucket := range buckets {
        str := fmt.Sprintf("Bucket Info: %+v\n", bucket)
        common.KTrace(str)
		//log.Println("CHECK BUCKET: ", bucket.Name)
		// Check if the current bucket has a configured lifecycle policy, skip otherwise
		l, ok := globalLifecycleSys.Get(bucket.Name)
		if !ok {
			continue
		}
        /*
        out, merr := json.MarshalIndent(l, "", "\t")
        if merr == nil {
            common.KTrace(string(out))
        } else {
            common.KTrace("Failed to marshall LifeCycle object")
        }
        */

		// Calculate the common prefix of all lifecycle rules
		//log.Println("1. CHECK BUCKET: ", bucket.Name, l)

		var prefixes []string
		for _, rule := range l.Rules {
			//log.Println(" PREFIX", rule.Prefix())
			prefixes = append(prefixes, rule.Prefix())
		}
		commonPrefix := lcp(prefixes)
                //log.Println("2. CHECK BUCKET: ", bucket.Name)

		// Allocate new results channel to receive ObjectInfo.
		objInfoCh := make(chan ObjectInfo)
        /*
        str = fmt.Sprintf("ObjInfoCh Info: %+v\n", objInfoCh)
        common.KTrace(str)
        fmt.Println(len(objInfoCh))
        */
		// Walk through all objects
		if err := objAPI.Walk(ctx, bucket.Name, commonPrefix, objInfoCh); err != nil {
			return err
		}
		for {
            common.KTrace("FOR")
			var objects []string
			for obj := range objInfoCh {
                common.KTrace("FOR obj loop: obj is delivered from objInfoCh")
                	//log.Println("3.0 CHECK BUCKET: ", bucket.Name, obj)

				if len(objects) == maxObjectList {
					// Reached maximum delete requests, attempt a delete for now.
					break
				}

				// Find the action that need to be executed
				if l.ComputeAction(obj.Name, obj.UserTags, obj.ModTime) == lifecycle.DeleteAction {
                    common.KTrace("+++ Delete Action")
					objects = append(objects, obj.Name)
				}
			}

			// Nothing to do.
			if len(objects) == 0 {
				break
			}

			waitForLowHTTPReq(int32(globalEndpoints.Nodes()))
	                //log.Println("3.1 CHECK BUCKET DEL: ", bucket.Name, objects)

			// Deletes a list of objects.
			deleteErrs, err := objAPI.DeleteObjects(ctx, bucket.Name, objects)

			if err != nil {
				logger.LogIf(ctx, err)
			} else {
	                        //log.Println("3.2 CHECK BUCKET: ", bucket.Name)

				for i := range deleteErrs {
					if deleteErrs[i] != nil {
						logger.LogIf(ctx, deleteErrs[i])
						continue
					}
					// Notify object deleted event.
					sendEvent(eventArgs{
						EventName:  event.ObjectRemovedDelete,
						BucketName: bucket.Name,
						Object: ObjectInfo{
							Name: objects[i],
						},
						Host: "Internal: [ILM-EXPIRY]",
					})
				}
			}
		}
	}
        //log.Println("4. CHECK BUCKET: ")

	return nil
}
