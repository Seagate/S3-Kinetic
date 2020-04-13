#ifndef LISTENER_H_
#define LISTENER_H_

namespace com {
namespace seagate {
namespace common {

class Listener {
public:
   Listener() {}
   virtual ~Listener() {}

   virtual void notify() = 0;
   virtual void notifyNewManifestSegments() { }
};

}  // namespace common
}  // namespace seagate
}  // namespace com

#endif
