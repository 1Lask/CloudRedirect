#include "pending_ops_journal.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>

int main() {
    namespace fs = std::filesystem;
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        ("cloudredirect-pending-journal-test-" + std::to_string(unique));
    fs::create_directories(root);

    PendingOpsJournal::Init(root.string());

    PendingOpsJournal::Entry session;
    session.machineName = "journal-test";
    session.clientId = 12345;
    session.timeLastUpdated = 1;

    // A crash after batch start leaves UploadInProgress. The next recovery
    // check must promote it to a durable recovery-required marker.
    PendingOpsJournal::RecordLaunchIntent(7654321, 480, session, false);
    PendingOpsJournal::RecordUploadBatchStart(7654321, 480);
    assert(PendingOpsJournal::HasPendingUpload(7654321, 480));

    // Launch handling clears the transient UploadPending notification, but
    // must not clear the durable marker protecting a possibly newer local save.
    PendingOpsJournal::ClearUploadPending(7654321, 480);
    assert(PendingOpsJournal::HasPendingUpload(7654321, 480));

    // A successful publication explicitly resolves recovery.
    PendingOpsJournal::ClearUploadRecoveryRequired(7654321, 480);
    assert(!PendingOpsJournal::HasPendingUpload(7654321, 480));

    // Publication failure creates a marker that survives launch bookkeeping.
    PendingOpsJournal::RecordUploadRecoveryRequired(7654321, 480);
    assert(PendingOpsJournal::HasPendingUpload(7654321, 480));
    PendingOpsJournal::ClearUploadPending(7654321, 480);
    assert(PendingOpsJournal::HasPendingUpload(7654321, 480));

    PendingOpsJournal::ClearPending(7654321, 480);
    assert(!PendingOpsJournal::HasPendingUpload(7654321, 480));

    std::error_code ec;
    fs::remove_all(root, ec);
    return 0;
}
