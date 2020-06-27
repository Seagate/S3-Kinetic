package common

func IncStr(str string) string {
    b := []byte(str)
    return string(IncByte(b))
}

func IncByte(bytes []byte) []byte {
    bytesLen := len(bytes)
    if bytesLen == 0 {
        return bytes
    }
    for idx := bytesLen - 1; idx >= 0; idx-- {
        bytes[idx]++
        if bytes[idx] > 0 {
            break
        }
    }
    return bytes
}
