#ifndef WINDOWSSYMLINK_H
#define WINDOWSSYMLINK_H

#include <QString>
#include <QStringList>

#ifdef Q_OS_WIN
#include <windows.h>

namespace WindowsSymlink {
using Launcher = DWORD (*)(const QString &parameters, HANDLE *process);
DWORD launchElevated(const QString &parameters, HANDLE *process);

// One approval per session. Failed/cancelled startup is also remembered so a
// batch never produces repeated UAC prompts. Only the helper is elevated.
class Session {
public:
    explicit Session(Launcher launcher = launchElevated) : launcher(launcher) {}
    ~Session();
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
    DWORD create(const QString &target, const QString &link);
private:
    DWORD start();
    void close();
    Launcher launcher;
    HANDLE pipe = INVALID_HANDLE_VALUE;
    HANDLE process = nullptr;
    bool attempted = false;
    DWORD failure = ERROR_SUCCESS;
};
// Both operations return a Win32 error code, with ERROR_SUCCESS on success.
using Operation = DWORD (*)(const QString &target, const QString &link);
using NativeCreate = BOOLEAN (WINAPI *)(LPCWSTR link, LPCWSTR target, DWORD flags);
// Native API boundary, injectable for path and compatibility-fallback tests.
DWORD createNative(const QString &target, const QString &link, DWORD flags, NativeCreate nativeCreate);
DWORD createDirect(const QString &target, const QString &link);
DWORD createElevated(const QString &target, const QString &link);
DWORD create(const QString &target, const QString &link,
             Operation direct = createDirect, Operation elevated = createElevated);
QString errorMessage(DWORD code);
// Returns -1 for normal application startup. Helper mode never starts Qt's GUI.
int runHelper(const QStringList &arguments);
int runHelperFromCommandLine();
}
#endif
#endif
