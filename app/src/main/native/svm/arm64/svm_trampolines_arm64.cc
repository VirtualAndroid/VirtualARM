//
// Created by 甘尧 on 2020-03-08.
//

#include "svm_trampolines_arm64.h"
#include "svm_context.h"

void SVM::A64::CodeCacheDispatcherTrampoline() {
    auto context = Context::Current()->GetCPUContext();
    auto brunch_vaddr = context.forward;
}

void SVM::A64::PageMissTrampoline() {
    auto context = Context::Current()->GetCPUContext();
    auto vaddr = context.forward;
}

void SVM::A64::ContextSwitchTrampoline() {
    auto context = Context::Current()->GetCPUContext();
    auto action = context.forward;
    auto data = context.forward_ext;
    switch (action) {

    }
}

void SVM::A64::SpecTrampoline() {
    auto context = Context::Current()->GetCPUContext();
    auto addr = context.forward;
}
