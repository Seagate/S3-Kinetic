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
    log.Printf("%s:%d:%s:%d: Exit", file, line, funcName, unix.Gettid())
    }
}

