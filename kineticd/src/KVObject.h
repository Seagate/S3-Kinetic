#ifndef _KV_OBJECT_H_
#define _KV_OBJECT_H_

//#include "minio_skinny_waist.h"

namespace com {
namespace seagate {
namespace kinetic {

class Key {
public:
    Key() {
        data_ = NULL;
        size_ = -1;
    }
	Key(char* key, int size) {
		data_ = key;
                size_ = size;
	}
	const char* data() const {
		return data_;
	}
	int size() {
		return size_;
	}
private:
	char* data_;
        int size_;
};

class MetaData {
public:
    MetaData() {
        size_ = -1;
        //version_ = "";
	//tag_ = NULL;
	algorithm_ = -1;
    }

    void setSize(int size) { size_ = size; }
    int size() { return size_; }
    void setTag(char* tag) { tag_ = string(tag); }
    const string& tag() { return tag_; }
    void setAlgorithm(int32_t algorithm) { algorithm_ = algorithm; }
    int32_t algorithm() { return algorithm_; }
    void setVersion(char* version) { version_ = string(version); }
    const string& version() { return version_; }

private:
    int size_;
    string version_;
    string tag_;
    int32_t algorithm_;
};

class Value {
public:
    Value() {
        data_ = NULL;
    }
	Value(char* val, int size) {
		data_ = val;
		metaData_.setSize(size);
	}
	void setMetaData(MetaData& metaData) {
		metaData_ = metaData;
	}
	const MetaData& metaData() {
		return metaData_;
	}
	void setTag(char* tag) { metaData_.setTag(tag); }
	void setVersion(char* version) { metaData_.setVersion(version); }
	void setAlgorithm(int32_t algorithm) { metaData_.setAlgorithm(algorithm); }
	const string& tag() { return metaData_.tag(); }
	const string& version() { return metaData_.version(); }
	int32_t algorithm() { return metaData_.algorithm(); }
	int size() { return metaData_.size(); }
        char* data() const { return data_; }

private:
	char* data_;
	MetaData metaData_;
};



class KVObject {
public:
/*
    KVObject(struct CKVObject* ckvObj) {
//KVObject::KVObject(struct CKVObject* ckvObj) {
    key_ = Key(ckvObj->key_, ckvObj->keySize_);
    value_ = Value(ckvObj->value_, ckvObj->valueSize_);
    value_.setTag(ckvObj->tag_);
    value_.setAlgorithm(ckvObj->algorithm_);
    value_.setVersion(ckvObj->version_); 
}
*/
    const string& version() { return value_.version(); }
    const string& tag() { return value_.tag(); }
    int32_t algorithm() { return value_.algorithm(); }
    int size() { return value_.size(); }
    const Value& value() { return value_; }
    const Key& key() { return key_; }

private:
	Key key_;
	Value value_;
};

}
}
}

#endif
