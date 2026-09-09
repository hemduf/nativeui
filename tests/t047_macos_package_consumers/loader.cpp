#include <dlfcn.h>
#include <objc/runtime.h>

#include <iostream>
#include <string>

namespace {

bool require_class(const std::string& name)
{
    if (objc_getClass(name.c_str()) == Nil) {
        std::cerr << "missing Objective-C runtime class: " << name << '\n';
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 5) {
        std::cerr << "usage: t047_consumer_loader <module-a> <prefix-a> <module-b> <prefix-b>\n";
        return 2;
    }

    void* const module_a = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module_a) {
        std::cerr << "failed to load first T047 consumer: " << dlerror() << '\n';
        return 3;
    }
    void* const module_b = dlopen(argv[3], RTLD_NOW | RTLD_LOCAL);
    if (!module_b) {
        std::cerr << "failed to load second T047 consumer: " << dlerror() << '\n';
        return 4;
    }

    const std::string prefix_a = argv[2];
    const std::string prefix_b = argv[4];
    for (const char* suffix : {"PuglWindow", "PuglWindowDelegate", "PuglWrapperView", "PuglOpenGLView"}) {
        if (!require_class(prefix_a + suffix) || !require_class(prefix_b + suffix)) {
            return 5;
        }
    }

    for (const char* generic : {"PuglWindow", "PuglWindowDelegate", "PuglWrapperView", "PuglOpenGLView"}) {
        if (objc_getClass(generic) != Nil) {
            std::cerr << "unexpected generic Pugl Objective-C runtime class: " << generic << '\n';
            return 6;
        }
    }

    (void)module_a;
    (void)module_b;
    return 0;
}
