#include "windowssymlink.h"

#ifdef Q_OS_WIN
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <shellapi.h>
#include <sddl.h>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>

namespace WindowsSymlink {
namespace {
const QString helperOption = QStringLiteral("--internal-create-symbolic-link");
const QString sessionOption = QStringLiteral("--internal-symbolic-link-session");
const QString pipePrefix = QStringLiteral("\\\\.\\pipe\\mvlink-");
constexpr DWORD maxRequestBytes = 1024 * 1024;

QString extendedPath(const QString &path)
{
    const QString native = QDir::toNativeSeparators(path);
    // Already-qualified namespaces must not be prefixed or normalized again.
    if (native.startsWith(QStringLiteral("\\\\?\\")) || native.startsWith(QStringLiteral("\\\\.\\")))
        return native;
    const bool unc = native.startsWith(QStringLiteral("\\\\"));
    const bool dos = native.size() >= 3 && native.at(0).isLetter()
        && native.at(1) == QLatin1Char(':') && native.at(2) == QLatin1Char('\\');
    if (!unc && !dos)
        return native; // Extended syntax requires an absolute path.
    // The extended namespace disables Win32's dot/separator normalization.
    const QString clean = QDir::toNativeSeparators(QDir::cleanPath(path));
    return unc ? QStringLiteral("\\\\?\\UNC\\") + clean.mid(2)
               : QStringLiteral("\\\\?\\") + clean;
}

bool validPaths(const QString &target, const QString &link)
{
    return QDir::isAbsolutePath(target) && QDir::isAbsolutePath(link)
        && !target.contains(QChar(0)) && !link.contains(QChar(0));
}

// Always finish/cancel outstanding I/O before its OVERLAPPED leaves scope.
DWORD finishIo(HANDLE pipe, HANDLE process, OVERLAPPED &operation, DWORD &bytes)
{
    const HANDLE handles[] = {operation.hEvent, process};
    const DWORD wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    if (wait != WAIT_OBJECT_0) {
        const DWORD error = wait == WAIT_OBJECT_0 + 1 ? ERROR_BROKEN_PIPE : GetLastError();
        CancelIoEx(pipe, &operation);
        GetOverlappedResult(pipe, &operation, &bytes, TRUE);
        return error;
    }
    return GetOverlappedResult(pipe, &operation, &bytes, FALSE) ? ERROR_SUCCESS : GetLastError();
}

DWORD transfer(HANDLE pipe, HANDLE process, void *buffer, DWORD size, bool write)
{
    OVERLAPPED operation = {};
    operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!operation.hEvent)
        return GetLastError();
    DWORD bytes = 0;
    const BOOL completed = write ? WriteFile(pipe, buffer, size, &bytes, &operation)
                                 : ReadFile(pipe, buffer, size, &bytes, &operation);
    DWORD code = completed ? ERROR_SUCCESS : GetLastError();
    if (code == ERROR_IO_PENDING)
        code = finishIo(pipe, process, operation, bytes);
    CloseHandle(operation.hEvent);
    return code == ERROR_SUCCESS && bytes != size ? ERROR_INVALID_DATA : code;
}

DWORD serve(const QString &name, DWORD parentPid)
{
    HANDLE pipe = CreateFileW(reinterpret_cast<LPCWSTR>(name.utf16()), GENERIC_READ | GENERIC_WRITE,
                              0, nullptr, OPEN_EXISTING, SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
    if (pipe == INVALID_HANDLE_VALUE)
        return GetLastError();
    ULONG serverPid = 0;
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!GetNamedPipeServerProcessId(pipe, &serverPid) || serverPid != parentPid
        || !SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr)) {
        CloseHandle(pipe);
        return ERROR_ACCESS_DENIED;
    }
    QByteArray buffer(maxRequestBytes, Qt::Uninitialized);
    DWORD count = 0;
    while (ReadFile(pipe, buffer.data(), maxRequestBytes, &count, nullptr)) {
        const QJsonDocument document = QJsonDocument::fromJson(QByteArray(buffer.constData(), count));
        const QJsonObject request = document.object();
        const QString target = request.value("target").toString();
        const QString link = request.value("link").toString();
        const DWORD result = request.size() == 2 && validPaths(target, link)
            ? createDirect(target, link) : ERROR_INVALID_PARAMETER;
        DWORD written = 0;
        if (!WriteFile(pipe, &result, sizeof(result), &written, nullptr) || written != sizeof(result))
            break;
    }
    // The parent's pipe handle closes on normal shutdown and on a crash.
    CloseHandle(pipe);
    return ERROR_SUCCESS;
}

// Quote for CommandLineToArgvW, never for cmd.exe. Escape trailing backslashes
// as well as embedded quotes so paths cannot become additional arguments.
QString quoteArgument(const QString &argument)
{
    QString result = "\"";
    int backslashes = 0;
    for (const QChar character : argument) {
        if (character == QLatin1Char('\\')) {
            ++backslashes;
            continue;
        }
        result += QString(backslashes * (character == QLatin1Char('"') ? 2 : 1), QLatin1Char('\\'));
        if (character == QLatin1Char('"'))
            result += QLatin1Char('\\');
        result += character;
        backslashes = 0;
    }
    result += QString(backslashes * 2, QLatin1Char('\\'));
    return result + QLatin1Char('"');
}
}

DWORD createDirect(const QString &target, const QString &link)
{
    const DWORD flags = QFileInfo(target).isDir() ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
    return createNative(target, link, flags, CreateSymbolicLinkW);
}

DWORD createNative(const QString &target, const QString &link, DWORD flags, NativeCreate nativeCreate)
{
    const QString nativeTarget = extendedPath(target);
    const QString nativeLink = extendedPath(link);
    // Developer Mode can permit this without elevation. Retry without the flag
    // for older Windows versions which do not recognize it.
    if (nativeCreate(reinterpret_cast<LPCWSTR>(nativeLink.utf16()),
                            reinterpret_cast<LPCWSTR>(nativeTarget.utf16()), flags | 0x2))
        return ERROR_SUCCESS;
    DWORD code = GetLastError();
    if (code == ERROR_INVALID_PARAMETER) {
        if (nativeCreate(reinterpret_cast<LPCWSTR>(nativeLink.utf16()),
                                reinterpret_cast<LPCWSTR>(nativeTarget.utf16()), flags))
            return ERROR_SUCCESS;
        code = GetLastError();
    }
    return code;
}

DWORD launchElevated(const QString &parameters, HANDLE *process)
{
    const QString executable = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    SHELLEXECUTEINFOW launch = {};
    launch.cbSize = sizeof(launch);
    launch.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    launch.lpVerb = L"runas";
    launch.lpFile = reinterpret_cast<LPCWSTR>(executable.utf16());
    launch.lpParameters = reinterpret_cast<LPCWSTR>(parameters.utf16());
    launch.nShow = SW_HIDE;
    if (!ShellExecuteExW(&launch))
        return GetLastError();
    if (!launch.hProcess)
        return ERROR_INVALID_HANDLE;
    *process = launch.hProcess;
    return ERROR_SUCCESS;
}

Session::~Session()
{
    close();
}

void Session::close()
{
    if (pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe);
        pipe = INVALID_HANDLE_VALUE;
    }
    if (process) {
        CloseHandle(process);
        process = nullptr;
    }
}

DWORD Session::start()
{
    const QString name = pipePrefix + QUuid::createUuid().toString(QUuid::WithoutBraces);
    // Allow the initiating user and administrators (also supporting UAC with
    // another administrator's credentials), but no remote pipe clients.
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return GetLastError();
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    QByteArray user(size, Qt::Uninitialized);
    const BOOL gotUser = GetTokenInformation(token, TokenUser, user.data(), size, &size);
    DWORD code = gotUser ? ERROR_SUCCESS : GetLastError();
    CloseHandle(token);
    if (code != ERROR_SUCCESS)
        return code;
    LPWSTR sid = nullptr;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(user.data())->User.Sid, &sid))
        return GetLastError();
    const QString acl = QStringLiteral("D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;%1)")
        .arg(QString::fromWCharArray(sid));
    LocalFree(sid);
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(reinterpret_cast<LPCWSTR>(acl.utf16()),
                                                              SDDL_REVISION_1, &descriptor, nullptr))
        return GetLastError();
    SECURITY_ATTRIBUTES attributes = {sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    pipe = CreateNamedPipeW(reinterpret_cast<LPCWSTR>(name.utf16()),
                            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                            1, maxRequestBytes, maxRequestBytes, 0, &attributes);
    code = pipe == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
    LocalFree(descriptor);
    if (code != ERROR_SUCCESS)
        return code;
    const QString parameters = sessionOption + " " + quoteArgument(name) + " "
        + QString::number(GetCurrentProcessId());
    code = launcher(parameters, &process);
    if (code != ERROR_SUCCESS)
        return code;
    OVERLAPPED operation = {};
    operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!operation.hEvent)
        return GetLastError();
    DWORD bytes = 0;
    const BOOL connected = ConnectNamedPipe(pipe, &operation);
    code = connected ? ERROR_SUCCESS : GetLastError();
    if (code == ERROR_IO_PENDING)
        code = finishIo(pipe, process, operation, bytes);
    else if (code == ERROR_PIPE_CONNECTED)
        code = ERROR_SUCCESS;
    CloseHandle(operation.hEvent);
    if (code != ERROR_SUCCESS)
        return code;
    ULONG clientPid = 0;
    if (!GetNamedPipeClientProcessId(pipe, &clientPid) || clientPid != GetProcessId(process))
        return ERROR_ACCESS_DENIED;
    return ERROR_SUCCESS;
}

DWORD Session::create(const QString &target, const QString &link)
{
    if (!validPaths(target, link))
        return ERROR_INVALID_PARAMETER;
    QByteArray request = QJsonDocument(QJsonObject{{"target", target}, {"link", link}}).toJson(QJsonDocument::Compact);
    if (request.size() > maxRequestBytes)
        return ERROR_INVALID_PARAMETER;
    if (!attempted) {
        attempted = true;
        failure = start();
        if (failure != ERROR_SUCCESS)
            close();
    }
    if (failure != ERROR_SUCCESS)
        return failure;
    failure = transfer(pipe, process, request.data(), DWORD(request.size()), true);
    DWORD result = ERROR_GEN_FAILURE;
    if (failure == ERROR_SUCCESS)
        failure = transfer(pipe, process, &result, sizeof(result), false);
    if (failure != ERROR_SUCCESS) {
        close();
        return failure;
    }
    return result;
}

DWORD createElevated(const QString &target, const QString &link)
{
    static Session session;
    return session.create(target, link);
}

DWORD create(const QString &target, const QString &link, Operation direct, Operation elevated)
{
    const QString absoluteTarget = QFileInfo(target).absoluteFilePath();
    const QString absoluteLink = QFileInfo(link).absoluteFilePath();
    const DWORD code = direct(absoluteTarget, absoluteLink);
    // Access denied, existing files, and missing directories are ordinary errors;
    // elevation is reserved for the symbolic-link privilege.
    return code == ERROR_PRIVILEGE_NOT_HELD ? elevated(absoluteTarget, absoluteLink) : code;
}

QString errorMessage(DWORD code)
{
    if (code == ERROR_CANCELLED)
        return QStringLiteral("Symbolic link creation cancelled at the Windows administrator prompt. Restart mvlink to try elevation again.");
    if (code == ERROR_BROKEN_PIPE || code == ERROR_PIPE_NOT_CONNECTED)
        return QStringLiteral("The symbolic-link helper has stopped. Restart mvlink to approve elevation again.");
    wchar_t *message = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<LPWSTR>(&message), 0, nullptr);
    const QString detail = message ? QString::fromWCharArray(message).trimmed() : QString();
    LocalFree(message);
    return QString("Windows error %1: %2").arg(code).arg(detail);
}

int runHelper(const QStringList &arguments)
{
    if (arguments.size() >= 2 && arguments.at(1) == sessionOption) {
        bool validPid = false;
        const DWORD pid = arguments.size() == 4 ? arguments.at(3).toUInt(&validPid) : 0;
        if (!validPid || !pid || !arguments.at(2).startsWith(pipePrefix)
            || QUuid(arguments.at(2).mid(pipePrefix.size())).isNull())
            return ERROR_INVALID_PARAMETER;
        return static_cast<int>(serve(arguments.at(2), pid));
    }
    if (arguments.size() < 2 || arguments.at(1) != helperOption)
        return -1;
    if (arguments.size() != 4 || !validPaths(arguments.at(2), arguments.at(3)))
        return ERROR_INVALID_PARAMETER;
    // Exactly one link, no overwrite, move, settings access, GUI, or re-elevation.
    return static_cast<int>(createDirect(arguments.at(2), arguments.at(3)));
}

int runHelperFromCommandLine()
{
    int count = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!argv)
        return static_cast<int>(GetLastError());
    QStringList arguments;
    for (int i = 0; i < count; ++i)
        arguments.append(QString::fromWCharArray(argv[i]));
    LocalFree(argv);
    return runHelper(arguments);
}
}
#endif
