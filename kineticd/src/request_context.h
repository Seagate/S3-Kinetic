#ifndef KINETIC_REQUEST_CONTEXT_H_
#define KINETIC_REQUEST_CONTEXT_H_

namespace com {
namespace seagate {
namespace kinetic {

class RequestContext {
public:
    RequestContext() { //int64 userId) {
        userId_ = 1; //userId;
	is_ssl = false;
	writeThrough_ = false;
	ignoreVersion_ = true;
    }

    bool ssl() { return is_ssl; }
    void setSsl(bool ssl) { is_ssl = ssl; }
    void setUserId(int64_t id) { userId_ = id; }
    int64_t userId() { return userId_; }
    bool writeThrough() { return writeThrough_; }
    void setWriteThrough(bool through) { writeThrough_ = through; }

    bool ignoreVersion() { return ignoreVersion_; }
    void setIgnoreVersion(bool ignore) { ignoreVersion_ = ignore; }

    bool is_ssl;
private:
    int64_t userId_;
    bool writeThrough_;
    bool ignoreVersion_;
};

} // namespace kinetic
} // namespace seagate
} // namespace com

#endif  // KINETIC_REQUEST_CONTEXT_H_
