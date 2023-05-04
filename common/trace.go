/*
 * Copyright (c) 2023 Seagate Technology LLC and/or its Affiliates
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
package common

import (
    "log"
    "runtime"
    "strings"
)

var traceOn bool = false
var savedFlags int
var savedPrefix string

func removePackageName(fullName string) string {
    elements := strings.Split(fullName, ".")
    return elements[len(elements) - 1]
}

func EnableTrace() {
    if traceOn {
        return
    }
    log.Println("========== Trace is ON ==========")
    savedFlags = log.Flags()
    savedPrefix = log.Prefix()
    log.SetPrefix("=== Trace: ")
    log.SetFlags(log.Ldate | log.Lmicroseconds)
    traceOn = true
}

func DisableTrace() {
    if !traceOn {
        return
    }
    log.SetPrefix(savedPrefix)
    log.SetFlags(savedFlags)
    traceOn = false 
    log.Println("========== Trace is OFF ==========")
}
func KTrace(msg string) string {
    if traceOn {
    function, file, line, _ := runtime.Caller(1)
    funcName := runtime.FuncForPC(function).Name()
    funcName = removePackageName(funcName)
    log.Printf("%s:%d:%s: %s", file, line, funcName, msg)
    }
    return ""
}

func KUntrace(unused string) {
    if traceOn {
    function, file, line, _ := runtime.Caller(1)
    funcName := runtime.FuncForPC(function).Name()
    funcName = removePackageName(funcName)
    log.Printf("%s:%d:%s: Exit", file, line, funcName)
    }
}

