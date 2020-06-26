package common

func NextStr(str string) string {
    b := []byte(str)
    return string(NextBytes(b))
}

func NextBytes(bytes []byte) []byte {
    bytesLen := len(bytes)
    if bytes[bytesLen - 1 ] < 255 {
        bytes[bytesLen - 1]++
    } else {
        idx := bytesLen - 1
        for idx > 0 && bytes[idx] == 255 {
            idx--
        }
        if idx >= 0 {
            bytes[idx]++
            idx++
        }
        for ; idx < bytesLen; idx++ {
            bytes[idx] = 0
        }
    }
    return bytes
}
