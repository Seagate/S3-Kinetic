package cmd

import (
	"syscall"
	"time"
)

type FileMode uint32

type kfileStat struct {
	name    string
	size    int64
	mode    FileMode
	modTime time.Time
	sys     syscall.Stat_t
}

func (kfs *kfileStat) Size() int64        { return kfs.size }
func (kfs *kfileStat) Mode() FileMode     { return kfs.mode }
func (kfs *kfileStat) ModTime() time.Time { return kfs.modTime }
func (kfs *kfileStat) Sys() interface{}   { return &kfs.sys }
