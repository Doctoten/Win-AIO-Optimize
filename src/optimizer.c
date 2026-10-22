#include "app.h"
#include "theme.h"
#include "packages.h"
#include <powrprof.h>
#include <aclapi.h>
#include <shlobj.h>
#include <sddl.h>
#include <string.h>
#include <stddef.h>
#include <windowsx.h>
#include <shlwapi.h>

/* Policy mappings and support limits: optimizer-notes.md. No scripts are executed. */
enum { A_DIAGNOSTICS, A_HISTORY, A_TRACKING, A_TIPS, A_EDGE, A_SEARCH,
       A_STORE_SEARCH, A_COPILOT, A_RECALL, A_CLICK, A_AI_SERVICE,
       A_WIDGETS, A_LOCK_WIDGETS, A_MEETNOW, A_DRAG, A_EXTENSIONS,
       A_3D, A_STANDBY, ACTION_COUNT };
typedef struct { const wchar_t *vi,*en,*detail; DWORD min_build,max_build; } Action;
static const Action actions[ACTION_COUNT]={
 {L"Giảm dữ liệu chẩn đoán / telemetry",L"Reduce diagnostic data / telemetry",L"Enterprise/Education: diagnostic data off. Home/Pro: Windows still requires basic diagnostic data; this cannot guarantee zero telemetry.",10240,0},
 {L"Tắt lịch sử hoạt động",L"Disable activity history",L"Stops activity history publishing and upload. Existing activity data is not deleted.",17134,0},
 {L"Tắt theo dõi mở ứng dụng và quảng cáo cá nhân",L"Disable app-launch tracking and targeted ads",L"Turns off Start app-launch tracking, advertising ID and tailored experiences for the current user.",10240,0},
 {L"Tắt mẹo, gợi ý và quảng cáo Windows",L"Disable Windows tips, suggestions and ads",L"Turns off consumer suggestions, notification tips, welcome recommendations and File Explorer provider ads. Some policies require Enterprise/Education.",10240,0},
 {L"Tắt quảng cáo, gợi ý và tin MSN trong Edge",L"Disable Edge ads, suggestions and MSN feed",L"Configures Edge policies for new-tab content, recommendations, rewards, shopping and personalized ads. Restart Edge and check edge://policy; managed profiles may override these settings.",10240,0},
 {L"Tắt Bing/AI trong tìm kiếm; gỡ Cortana",L"Disable Bing/AI web search; remove Cortana",L"Disables web search and search highlights. Removes the optional Cortana app for this user only. The Windows Search system component is preserved. App data cannot be restored by the settings snapshot.",19041,0},
 {L"Tắt gợi ý ứng dụng Store trong tìm kiếm (W11)",L"Disable Store app suggestions in search (W11)",L"Blocks data access to this user's existing Store suggestion cache (store.db). This is a compatibility workaround, not a Microsoft policy, and may affect Store recommendations. Original file permissions are saved. Missing caches are skipped; no file is deleted.",22621,0},
 {L"Tắt và gỡ Microsoft Copilot",L"Disable and remove Microsoft Copilot",L"Disables legacy Windows Copilot and removes the Microsoft.Copilot app for the current user. Does not remove Microsoft 365 or Edge. Removal and app data are not covered by the settings snapshot.",19045,0},
 {L"Tắt Recall; xóa ảnh chụp đã lưu (W11)",L"Disable Recall; delete saved snapshots (W11)",L"Disables Recall enablement and saving snapshots. WINDOWS DELETES EXISTING RECALL SNAPSHOTS. This data cannot be restored. Requires supported Windows 11 24H2 policy and may require restart.",26100,0},
 {L"Tắt Click to Do / phân tích màn hình (W11)",L"Disable Click to Do screen analysis (W11)",L"Disables Click to Do and its text/image analysis entry points. Requires the policy in installed WindowsCopilot.admx and a supported edition; availability varies by Windows update.",26100,0},
 {L"Ngăn WSAIFabricSvc tự khởi động (W11)",L"Prevent WSAIFabricSvc auto-start (W11)",L"Changes the existing Windows AI Fabric service to Manual. It is not forcibly stopped; Windows can still start it on demand. Original start configuration is saved.",22000,0},
 {L"Tắt widgets / tin tức trên thanh tác vụ",L"Disable taskbar widgets / news",L"Hides the current-user taskbar button and configures the supported widget/news policy. Restart Explorer by signing out to refresh; no process is killed.",19041,0},
 {L"Tắt widgets trên màn hình khóa",L"Disable lock-screen widgets",L"Uses DisableWidgetsOnLockScreen when the installed policy supports it. Other builds are reported unsupported, rather than disabling the lock screen itself.",22000,0},
 {L"Ẩn biểu tượng Meet Now (W10)",L"Hide Meet Now icon (W10)",L"Hides the Meet Now taskbar icon for the current user. Windows 10 only.",19041,21999},
 {L"Tắt Drag / Drop Tray (W11)",L"Disable Drag / Drop Tray (W11)",L"Turns off the current-user drag-to-share tray. Requires Windows 11 with the supported CDP DragTrayEnabled preference or the released tray update.",26100,0},
 {L"Hiện phần mở rộng tên file",L"Show known file extensions",L"Shows extensions such as .exe, .txt and .pdf in File Explorer for the current user.",10240,0},
 {L"Ẩn thư mục 3D Objects trong This PC (W10)",L"Hide 3D Objects in This PC (W10)",L"Sets the 3D Objects known-folder ThisPCPolicy to Hide. Files and the folder itself are not deleted. Windows 10 only.",16299,21999},
 {L"Tắt mạng khi Modern Standby (W11, nếu hỗ trợ)",L"Disable Modern Standby networking (W11, if supported)",L"Disables standby connectivity on AC and battery for the active power plan only when S0 low-power idle and the setting exist. Sleep notifications and remote access may be unavailable. Saves the original values and scheme.",22000,0}
};
static const wchar_t *details_vi[ACTION_COUNT]={
 L"Enterprise/Education: tắt dữ liệu chẩn đoán. Home/Pro: Windows vẫn yêu cầu dữ liệu chẩn đoán cơ bản; không thể bảo đảm tắt toàn bộ telemetry.",
 L"Ngừng ghi và tải lịch sử hoạt động lên máy chủ. Không xóa lịch sử đã có.",
 L"Tắt theo dõi mở ứng dụng trong Start, mã quảng cáo và trải nghiệm cá nhân hóa cho tài khoản hiện tại.",
 L"Tắt gợi ý tiêu dùng, mẹo thông báo, lời chào sau cập nhật và quảng cáo nhà cung cấp trong Explorer. Một số chính sách chỉ hỗ trợ Enterprise/Education.",
 L"Tắt nội dung trang tab mới, gợi ý, Rewards, trợ lý mua sắm và quảng cáo cá nhân trong Edge. Khởi động lại Edge và kiểm tra edge://policy; cấu hình do tổ chức quản lý có thể ghi đè.",
 L"Tắt kết quả web và nội dung nổi bật trong tìm kiếm. Gỡ ứng dụng Cortana tùy chọn cho tài khoản hiện tại, giữ bộ máy Windows Search. Bản sao cài đặt không khôi phục dữ liệu ứng dụng.",
 L"Chặn đọc/ghi cache gợi ý Store hiện có (store.db) cho tài khoản hiện tại. Đây là cách xử lý tương thích, không phải chính sách Microsoft; có thể ảnh hưởng gợi ý trong Store. Lưu quyền truy cập gốc; bỏ qua nếu file không có và không xóa file.",
 L"Tắt Copilot tích hợp kiểu cũ và gỡ ứng dụng Microsoft.Copilot cho tài khoản hiện tại. Không gỡ Microsoft 365 hoặc Edge. Bản sao cài đặt không khôi phục ứng dụng và dữ liệu đã gỡ.",
 L"Tắt Recall và tính năng lưu ảnh chụp. WINDOWS SẼ XÓA ẢNH CHỤP RECALL ĐÃ LƯU; dữ liệu này không thể khôi phục. Cần chính sách được hỗ trợ của Windows 11 24H2 và có thể cần khởi động lại.",
 L"Tắt Click to Do và các điểm truy cập phân tích văn bản/hình ảnh. Cần chính sách trong WindowsCopilot.admx và bản Windows phù hợp; phụ thuộc cập nhật Windows.",
 L"Chuyển dịch vụ Windows AI Fabric hiện có sang Manual. Không buộc dừng dịch vụ; Windows vẫn có thể khởi chạy khi cần. Lưu cấu hình khởi động gốc để khôi phục.",
 L"Ẩn nút widgets/tin tức trên thanh tác vụ của tài khoản hiện tại và đặt chính sách được hỗ trợ. Đăng xuất để làm mới Explorer; không cưỡng ép dừng tiến trình.",
 L"Dùng chính sách DisableWidgetsOnLockScreen khi Windows có hỗ trợ. Các bản Windows khác được báo không hỗ trợ; không vô hiệu hóa màn hình khóa.",
 L"Ẩn biểu tượng Meet Now trên thanh tác vụ cho tài khoản hiện tại. Chỉ dành cho Windows 10.",
 L"Tắt khay kéo-thả để chia sẻ file của tài khoản hiện tại. Cần Windows 11 có giá trị CDP DragTrayEnabled hoặc cập nhật Drag/Drop Tray được hỗ trợ.",
 L"Hiện phần mở rộng như .exe, .txt và .pdf trong File Explorer cho tài khoản hiện tại.",
 L"Đặt ThisPCPolicy của thư mục 3D Objects thành Hide. Không xóa thư mục hoặc file bên trong. Chỉ dành cho Windows 10.",
 L"Tắt mạng khi ngủ trên nguồn AC và pin cho chế độ nguồn hiện tại, chỉ khi hỗ trợ S0 low-power idle và tùy chọn này. Có thể mất thông báo/truy cập từ xa khi ngủ. Lưu giá trị và chế độ nguồn gốc."
};

#define CU HKEY_CURRENT_USER
#define LM HKEY_LOCAL_MACHINE
#define CURRENT L"Software\\Microsoft\\Windows\\CurrentVersion\\"
#define POLICY L"Software\\Policies\\Microsoft\\Windows\\"
#define EXPLORER CURRENT L"Explorer\\Advanced"
/* edition: 0 preference/all; 1 Pro/Enterprise/Education; 2 Enterprise/Education */
typedef struct { unsigned action; HKEY root; const wchar_t *key,*name; DWORD value;
                 unsigned edition; DWORD min_build,max_build; const wchar_t *text; } RegOp;
#define R(a,r,k,n,v,e,lo,hi) {a,r,k,n,v,e,lo,hi,NULL}
static const RegOp reg_ops[]={
 R(A_DIAGNOSTICS,LM,POLICY L"DataCollection",L"AllowTelemetry",0,0,10240,0),
 R(A_DIAGNOSTICS,LM,POLICY L"DataCollection",L"DoNotShowFeedbackNotifications",1,1,10240,0),
 R(A_HISTORY,LM,POLICY L"System",L"EnableActivityFeed",0,1,17134,0),
 R(A_HISTORY,LM,POLICY L"System",L"PublishUserActivities",0,1,17134,0),
 R(A_HISTORY,LM,POLICY L"System",L"UploadUserActivities",0,1,17134,0),
 R(A_TRACKING,CU,EXPLORER,L"Start_TrackProgs",0,0,10240,0),
 R(A_TRACKING,CU,CURRENT L"AdvertisingInfo",L"Enabled",0,0,10240,0),
 R(A_TRACKING,CU,CURRENT L"Privacy",L"TailoredExperiencesWithDiagnosticDataEnabled",0,0,15063,0),
 R(A_TRACKING,CU,POLICY L"CloudContent",L"DisableTailoredExperiencesWithDiagnosticData",1,1,15063,0),
 R(A_TIPS,LM,POLICY L"CloudContent",L"DisableWindowsConsumerFeatures",1,2,10240,0),
 R(A_TIPS,LM,POLICY L"CloudContent",L"DisableSoftLanding",1,2,10240,0),
 R(A_TIPS,CU,CURRENT L"ContentDeliveryManager",L"SoftLandingEnabled",0,0,10240,0),
 R(A_TIPS,CU,CURRENT L"ContentDeliveryManager",L"SystemPaneSuggestionsEnabled",0,0,10240,0),
 R(A_TIPS,CU,CURRENT L"ContentDeliveryManager",L"SilentInstalledAppsEnabled",0,0,10240,0),
 R(A_TIPS,CU,CURRENT L"ContentDeliveryManager",L"SubscribedContent-338388Enabled",0,0,10240,0),
 R(A_TIPS,CU,CURRENT L"ContentDeliveryManager",L"SubscribedContent-338389Enabled",0,0,10240,0),
 R(A_TIPS,CU,CURRENT L"ContentDeliveryManager",L"SubscribedContent-338393Enabled",0,0,10240,0),
 R(A_TIPS,CU,CURRENT L"ContentDeliveryManager",L"SubscribedContent-353694Enabled",0,0,10240,0),
 R(A_TIPS,CU,CURRENT L"ContentDeliveryManager",L"SubscribedContent-353696Enabled",0,0,10240,0),
 R(A_TIPS,CU,CURRENT L"ContentDeliveryManager",L"SubscribedContent-310093Enabled",0,0,10240,0),
 R(A_TIPS,CU,CURRENT L"ContentDeliveryManager",L"RotatingLockScreenOverlayEnabled",0,0,10240,0),
 R(A_TIPS,CU,CURRENT L"UserProfileEngagement",L"ScoobeSystemSettingEnabled",0,0,10240,0),
 R(A_TIPS,CU,EXPLORER,L"ShowSyncProviderNotifications",0,0,10240,0),
 R(A_TIPS,CU,EXPLORER,L"Start_IrisRecommendations",0,0,22000,0),
 R(A_EDGE,LM,L"Software\\Policies\\Microsoft\\Edge",L"NewTabPageContentEnabled",0,0,10240,0),
 R(A_EDGE,LM,L"Software\\Policies\\Microsoft\\Edge",L"ShowRecommendationsEnabled",0,0,10240,0),
 R(A_EDGE,LM,L"Software\\Policies\\Microsoft\\Edge",L"SpotlightExperiencesAndRecommendationsEnabled",0,0,10240,0),
 R(A_EDGE,LM,L"Software\\Policies\\Microsoft\\Edge",L"PersonalizationReportingEnabled",0,0,10240,0),
 R(A_EDGE,LM,L"Software\\Policies\\Microsoft\\Edge",L"ShowMicrosoftRewards",0,0,10240,0),
 R(A_EDGE,LM,L"Software\\Policies\\Microsoft\\Edge",L"EdgeShoppingAssistantEnabled",0,0,10240,0),
 R(A_EDGE,LM,L"Software\\Policies\\Microsoft\\Edge",L"NewTabPageHideDefaultTopSites",1,0,10240,0),
 R(A_SEARCH,CU,POLICY L"Explorer",L"DisableSearchBoxSuggestions",1,1,19041,0),
 R(A_SEARCH,LM,POLICY L"Windows Search",L"ConnectedSearchUseWeb",0,2,17134,0),
 R(A_SEARCH,LM,POLICY L"Windows Search",L"AllowCortana",0,1,10240,0),
 R(A_SEARCH,LM,POLICY L"Windows Search",L"EnableDynamicContentInWSB",0,1,19044,0),
 R(A_SEARCH,CU,CURRENT L"Search",L"BingSearchEnabled",0,0,10240,0),
 R(A_SEARCH,CU,CURRENT L"Search",L"CortanaConsent",0,0,10240,0),
 R(A_SEARCH,CU,CURRENT L"SearchSettings",L"IsDynamicSearchBoxEnabled",0,0,19044,0),
 R(A_COPILOT,CU,POLICY L"WindowsCopilot",L"TurnOffWindowsCopilot",1,1,19045,0),
 R(A_COPILOT,CU,EXPLORER,L"ShowCopilotButton",0,0,22000,0),
 R(A_RECALL,LM,POLICY L"WindowsAI",L"AllowRecallEnablement",0,1,26100,0),
 R(A_RECALL,LM,POLICY L"WindowsAI",L"DisableAIDataAnalysis",1,1,26100,0),
 R(A_CLICK,LM,POLICY L"WindowsAI",L"DisableClickToDo",1,1,26100,0),
 R(A_WIDGETS,LM,L"Software\\Policies\\Microsoft\\Dsh",L"AllowNewsAndInterests",0,1,22000,0),
 R(A_WIDGETS,CU,EXPLORER,L"TaskbarDa",0,0,22000,0),
 R(A_WIDGETS,LM,POLICY L"Windows Feeds",L"EnableFeeds",0,1,19041,21999),
 R(A_WIDGETS,CU,CURRENT L"Feeds",L"ShellFeedsTaskbarViewMode",2,0,19041,21999),
 R(A_LOCK_WIDGETS,LM,L"Software\\Policies\\Microsoft\\Dsh",L"DisableWidgetsOnLockScreen",1,1,22000,0),
 R(A_MEETNOW,CU,CURRENT L"Policies\\Explorer",L"HideSCAMeetNow",1,0,19041,21999),
 R(A_DRAG,CU,CURRENT L"CDP",L"DragTrayEnabled",0,0,26100,0),
 R(A_EXTENSIONS,CU,EXPLORER,L"HideFileExt",0,0,10240,0),
 {A_3D,LM,CURRENT L"Explorer\\FolderDescriptions\\{31C0DD25-9439-4F12-BF41-7FF4EDA38722}\\PropertyBag",L"ThisPCPolicy",0,0,16299,21999,L"Hide"}
};
#define REG_COUNT ((unsigned)ARRAYSIZE(reg_ops))
#define OP_SERVICE REG_COUNT
#define OP_POWER (REG_COUNT+1)
#define OP_STORE_ACL (REG_COUNT+2)
#define SNAP_MAX (REG_COUNT+3)
#define SNAP_DATA 2048
#define SNAP_VERSION 0x6000001u
typedef struct { DWORD id,exists,type,size; BYTE data[SNAP_DATA]; } SavedValue;
typedef struct { DWORD magic,version,count,build; SYSTEMTIME time; SavedValue values[SNAP_MAX]; } Snapshot;
typedef struct { DWORD start,delayed; } ServiceState;
typedef struct { GUID scheme; DWORD ac,dc; } PowerState;
/* Store a bounded SDDL string, never raw pointers/offsets from an untrusted file. */
typedef struct { DWORD volume,index_high,index_low,sd_size; wchar_t descriptor[(SNAP_DATA-16)/2]; } StoreSecurity;
static const GUID standby_network={0xf15576e8,0x98b7,0x4186,{0xb9,0x44,0xea,0xfa,0x66,0x44,0x02,0xd9}};
static const GUID no_subgroup={0xfea3413e,0x7e05,0x4911,{0x9a,0x71,0x70,0x03,0x31,0xf1,0xc2,0x94}};
typedef struct { DWORD build,ubr; unsigned edition; } Platform;
typedef struct {
    HWND window,list,detail,status;
    HWND viewport,checks[ACTION_COUNT],group_labels[6];
    HFONT heading_font,section_font;
    RECT cards[6],rows[ACTION_COUNT];
    BOOL supported[ACTION_COUNT],visible[ACTION_COUNT];
    wchar_t reasons[ACTION_COUNT][160];
    int scroll,content_height,detail_action;
    BOOL bloat,advanced,selected[ACTION_COUNT],has_result,initialized;
    PackageItem packages[PACKAGE_ITEMS_MAX]; unsigned package_count;
    unsigned mode,done,failed,skipped;
    Snapshot *restore;
} OptimizerWindow;
enum { ID_LIST=501, ID_APPLY, ID_RESTORE, ID_BLOAT, ID_REFRESH, ID_DETAIL, ID_STATUS, ID_NONE, ID_HEADING,
       ID_SUBHEADING, ID_SEARCH, ID_REG_BACKUP, ID_RESTORE_POINT, ID_RECOVERY_HEADING, ID_RECOVERY_NOTE, ID_VIEWPORT, ID_ADVANCED };
#define ID_ACTION_BASE 800
#define WM_SETUP_FOCUS (WM_APP + 10)
typedef struct { const wchar_t *vi,*en; unsigned actions[5],count; } SetupGroup;
static const SetupGroup setup_groups[] = {
    {L"Quyền riêng tư & nội dung", L"Privacy & suggested content", {A_DIAGNOSTICS,A_HISTORY,A_TRACKING,A_TIPS,A_EDGE},5},
    {L"Trí tuệ nhân tạo", L"AI features", {A_COPILOT,A_RECALL,A_CLICK,A_AI_SERVICE},4},
    {L"Start & tìm kiếm", L"Start & search", {A_SEARCH,A_STORE_SEARCH},2},
    {L"Hệ thống", L"System", {A_DRAG,A_STANDBY},2},
    {L"Thanh tác vụ & màn hình khóa", L"Taskbar & lock screen", {A_WIDGETS,A_LOCK_WIDGETS,A_MEETNOW},3},
    {L"File Explorer", L"File Explorer", {A_EXTENSIONS,A_3D},2}
};

static DWORD read_dword(HKEY root,const wchar_t *key,const wchar_t *name,DWORD fallback) {
    DWORD v=fallback,n=sizeof(v);
    if (RegGetValueW(root,key,name,RRF_RT_REG_DWORD|RRF_SUBKEY_WOW6464KEY,NULL,&v,&n)!=ERROR_SUCCESS) return fallback;
    return v;
}
static Platform platform(void) {
    Platform p={app_windows_build(),0,0};
    p.ubr=read_dword(LM,L"Software\\Microsoft\\Windows NT\\CurrentVersion",L"UBR",0);
    wchar_t edition[128]=L""; DWORD n=sizeof(edition);
    if (RegGetValueW(LM,L"Software\\Microsoft\\Windows NT\\CurrentVersion",L"EditionID",RRF_RT_REG_SZ,NULL,edition,&n)==ERROR_SUCCESS) {
        if (wcsstr(edition,L"Enterprise") || wcsstr(edition,L"Education")) p.edition=2;
        else if (wcsstr(edition,L"Professional")) p.edition=1;
    }
    return p;
}
static BOOL admx_contains(const wchar_t *file,const char *token) {
    wchar_t path[MAX_PATH];
    if (!GetWindowsDirectoryW(path,ARRAYSIZE(path))) return FALSE;
    if (FAILED(StringCchCatW(path,ARRAYSIZE(path),L"\\PolicyDefinitions\\")) || FAILED(StringCchCatW(path,ARRAYSIZE(path),file))) return FALSE;
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
    if (f==INVALID_HANDLE_VALUE) return FALSE;
    DWORD size=GetFileSize(f,NULL),got=0;
    BOOL found=FALSE;
    if (size>0 && size<2*1024*1024) {
        char *data=calloc((size_t)size+2,1);
        if (data && ReadFile(f,data,size,&got,NULL) && got==size) {
            if (size>=2 && (BYTE)data[0]==0xff && (BYTE)data[1]==0xfe) {
                wchar_t wide[128];
                if (MultiByteToWideChar(CP_UTF8,0,token,-1,wide,ARRAYSIZE(wide))) found=wcsstr((const wchar_t *)(data+2),wide)!=NULL;
            } else found=strstr(data,token)!=NULL;
        }
        free(data);
    }
    CloseHandle(f); return found;
}
static DWORD service_read(ServiceState *state) {
    SC_HANDLE scm=OpenSCManagerW(NULL,NULL,SC_MANAGER_CONNECT);
    if (!scm) return GetLastError();
    SC_HANDLE svc=OpenServiceW(scm,L"WSAIFabricSvc",SERVICE_QUERY_CONFIG);
    if (!svc) { DWORD err=GetLastError(); CloseServiceHandle(scm); return err; }
    DWORD needed=0,err=ERROR_SUCCESS;
    QueryServiceConfigW(svc,NULL,0,&needed);
    QUERY_SERVICE_CONFIGW *cfg=malloc(needed);
    if (!cfg) err=ERROR_NOT_ENOUGH_MEMORY;
    else if (!QueryServiceConfigW(svc,cfg,needed,&needed)) err=GetLastError();
    else {
        state->start=cfg->dwStartType;
        SERVICE_DELAYED_AUTO_START_INFO delayed={0};
        if (!QueryServiceConfig2W(svc,SERVICE_CONFIG_DELAYED_AUTO_START_INFO,(BYTE *)&delayed,sizeof(delayed),&needed)) err=GetLastError();
        else state->delayed=delayed.fDelayedAutostart;
    }
    free(cfg); CloseServiceHandle(svc); CloseServiceHandle(scm); return err;
}
static DWORD service_write(const ServiceState *state) {
    ServiceState current={0}; DWORD read_error=service_read(&current);
    if (read_error) return read_error;
    if (state->start<SERVICE_AUTO_START || state->start>SERVICE_DISABLED) return ERROR_INVALID_DATA;
    if (state->start==current.start && state->delayed==current.delayed) return ERROR_SUCCESS;
    SC_HANDLE scm=OpenSCManagerW(NULL,NULL,SC_MANAGER_CONNECT);
    if (!scm) return GetLastError();
    SC_HANDLE svc=OpenServiceW(scm,L"WSAIFabricSvc",SERVICE_CHANGE_CONFIG);
    if (!svc) { DWORD err=GetLastError(); CloseServiceHandle(scm); return err; }
    DWORD err=ERROR_SUCCESS;
    /* Clear delayed auto-start before switching away from automatic. */
    SERVICE_DELAYED_AUTO_START_INFO delayed={FALSE};
    if (current.delayed && !ChangeServiceConfig2W(svc,SERVICE_CONFIG_DELAYED_AUTO_START_INFO,&delayed)) err=GetLastError();
    if (!err && !ChangeServiceConfigW(svc,SERVICE_NO_CHANGE,state->start,SERVICE_NO_CHANGE,NULL,NULL,NULL,NULL,NULL,NULL,NULL)) err=GetLastError();
    if (!err && state->start==SERVICE_AUTO_START && state->delayed) {
        delayed.fDelayedAutostart=TRUE;
        if (!ChangeServiceConfig2W(svc,SERVICE_CONFIG_DELAYED_AUTO_START_INFO,&delayed)) err=GetLastError();
    }
    CloseServiceHandle(svc); CloseServiceHandle(scm); return err;
}
static DWORD power_read(PowerState *state) {
    SYSTEM_POWER_CAPABILITIES caps={0};
    /* Windows SDK: AoAc is byte 20. Older MinGW headers name it spare2[2].
       Assert the ABI boundary instead of depending on a toolchain-specific name. */
    _Static_assert(offsetof(SYSTEM_POWER_CAPABILITIES,DiskSpinDown)==21,"Unexpected power capabilities layout");
    if (!GetPwrCapabilities(&caps) || !((const BYTE *)&caps)[20]) return ERROR_NOT_SUPPORTED;
    GUID *active=NULL; DWORD err=PowerGetActiveScheme(NULL,&active);
    if (err) return err;
    state->scheme=*active; LocalFree(active);
    err=PowerReadACValueIndex(NULL,&state->scheme,&no_subgroup,&standby_network,&state->ac);
    if (!err) err=PowerReadDCValueIndex(NULL,&state->scheme,&no_subgroup,&standby_network,&state->dc);
    return err;
}
static DWORD power_write(const PowerState *state) {
    DWORD err=PowerWriteACValueIndex(NULL,&state->scheme,&no_subgroup,&standby_network,state->ac);
    if (!err) err=PowerWriteDCValueIndex(NULL,&state->scheme,&no_subgroup,&standby_network,state->dc);
    /* Refresh only if still active; restore must not switch the user's plan. */
    GUID *active=NULL;
    if (!err) {
        err=PowerGetActiveScheme(NULL,&active);
        if (!err && IsEqualGUID(active,&state->scheme)) err=PowerSetActiveScheme(NULL,active);
    }
    if (active) LocalFree(active);
    return err;
}
static DWORD store_open(HANDLE *file) {
    wchar_t path[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,SHGFP_TYPE_CURRENT,path))) return ERROR_PATH_NOT_FOUND;
    if (FAILED(StringCchCatW(path,ARRAYSIZE(path),L"\\Packages\\Microsoft.WindowsStore_8wekyb3d8bbwe\\LocalState\\store.db"))) return ERROR_BUFFER_OVERFLOW;
    /* Reject every redirecting component before operating on the exact cache. */
    for (wchar_t *p=path+3;*p;++p) if (*p==L'\\') {
        *p=0; DWORD attributes=GetFileAttributesW(path); *p=L'\\';
        if (attributes==INVALID_FILE_ATTRIBUTES) return GetLastError();
        if (attributes&FILE_ATTRIBUTE_REPARSE_POINT) return ERROR_NOT_SUPPORTED;
    }
    *file=CreateFileW(path,READ_CONTROL|WRITE_DAC,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if (*file==INVALID_HANDLE_VALUE) return GetLastError();
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(*file,&info)) { DWORD err=GetLastError(); CloseHandle(*file); *file=INVALID_HANDLE_VALUE; return err; }
    if ((info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)) || info.nNumberOfLinks!=1) {
        CloseHandle(*file); *file=INVALID_HANDLE_VALUE; return ERROR_NOT_SUPPORTED;
    }
    return ERROR_SUCCESS;
}
static DWORD store_capture(StoreSecurity *saved) {
    HANDLE file=INVALID_HANDLE_VALUE; DWORD err=store_open(&file);
    if (err) return err;
    BY_HANDLE_FILE_INFORMATION info;
    PSECURITY_DESCRIPTOR sd=NULL; PACL acl=NULL;
    if (!GetFileInformationByHandle(file,&info)) err=GetLastError();
    else err=GetSecurityInfo(file,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,NULL,NULL,&acl,NULL,&sd);
    if (!err) {
        LPWSTR sddl=NULL; ULONG chars=0;
        if (!ConvertSecurityDescriptorToStringSecurityDescriptorW(sd,SDDL_REVISION_1,DACL_SECURITY_INFORMATION,&sddl,&chars)) err=GetLastError();
        else if (chars>ARRAYSIZE(saved->descriptor)) err=ERROR_BUFFER_OVERFLOW;
        else {
            ZeroMemory(saved,sizeof(*saved)); saved->volume=info.dwVolumeSerialNumber;
            saved->index_high=info.nFileIndexHigh; saved->index_low=info.nFileIndexLow;
            saved->sd_size=chars; memcpy(saved->descriptor,sddl,(size_t)chars*sizeof(wchar_t));
        }
        LocalFree(sddl);
    }
    if (sd) LocalFree(sd);
    CloseHandle(file); return err;
}
static DWORD store_write(const StoreSecurity *saved,BOOL restore) {
    HANDLE file=INVALID_HANDLE_VALUE; DWORD err=store_open(&file);
    if (err) return err;
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(file,&info)) err=GetLastError();
    else if (info.dwVolumeSerialNumber!=saved->volume || info.nFileIndexHigh!=saved->index_high || info.nFileIndexLow!=saved->index_low) err=ERROR_FILE_INVALID;
    PACL original=NULL,updated=NULL; BOOL present=FALSE,defaulted=FALSE;
    PSECURITY_DESCRIPTOR descriptor=NULL;
    SECURITY_DESCRIPTOR_CONTROL control=0; DWORD revision=0;
    if (!err && !ConvertStringSecurityDescriptorToSecurityDescriptorW(saved->descriptor,SDDL_REVISION_1,&descriptor,NULL)) err=GetLastError();
    if (!err && (!GetSecurityDescriptorDacl(descriptor,&present,&original,&defaulted) ||
                 !GetSecurityDescriptorControl(descriptor,&control,&revision))) err=ERROR_INVALID_SECURITY_DESCR;
    if (!err && !restore) {
        BYTE everyone[SECURITY_MAX_SID_SIZE]; DWORD sid_size=sizeof(everyone);
        if (!CreateWellKnownSid(WinWorldSid,NULL,everyone,&sid_size)) err=GetLastError();
        else {
            EXPLICIT_ACCESSW entry={0};
            /* Preserve READ_CONTROL/WRITE_DAC so undo needs no ownership takeover. */
            entry.grfAccessPermissions=FILE_READ_DATA|FILE_WRITE_DATA|FILE_APPEND_DATA;
            entry.grfAccessMode=DENY_ACCESS; entry.grfInheritance=NO_INHERITANCE;
            BuildTrusteeWithSidW(&entry.Trustee,everyone);
            err=SetEntriesInAclW(1,&entry,original,&updated);
        }
    }
    if (!err) {
        SECURITY_INFORMATION flags=DACL_SECURITY_INFORMATION;
        flags|=(control&SE_DACL_PROTECTED)?PROTECTED_DACL_SECURITY_INFORMATION:UNPROTECTED_DACL_SECURITY_INFORMATION;
        err=SetSecurityInfo(file,SE_FILE_OBJECT,flags,NULL,NULL,restore?original:updated,NULL);
    }
    if (updated) LocalFree(updated);
    LocalFree(descriptor);
    CloseHandle(file); return err;
}
static BOOL action_supported(unsigned a,const Platform *p,const wchar_t **reason) {
    *reason=tr(L"Có thể áp dụng",L"Available");
    if (p->build<actions[a].min_build || (actions[a].max_build && p->build>actions[a].max_build)) {
        *reason=actions[a].max_build?tr(L"Chỉ dành cho Windows 10",L"Windows 10 only"):tr(L"Cần bản Windows mới hơn",L"Newer Windows required"); return FALSE;
    }
    if (a==A_RECALL && (!p->edition || (p->build==26100 && p->ubr<3915) || !admx_contains(L"WindowsCopilot.admx","AllowRecallEnablement"))) {
        *reason=tr(L"Cần chính sách 24H2 / Pro trở lên",L"Requires supported 24H2 policy / Pro+"); return FALSE;
    }
    if (a==A_CLICK && (!p->edition || !admx_contains(L"WindowsCopilot.admx","DisableClickToDo"))) {
        *reason=tr(L"Chưa hỗ trợ chính sách / cần Pro+",L"Policy unavailable / Pro+ required"); return FALSE;
    }
    if (a==A_LOCK_WIDGETS && (!p->edition || !admx_contains(L"NewsAndInterests.admx","DisableWidgetsOnLockScreen"))) {
        *reason=tr(L"Chưa hỗ trợ chính sách màn hình khóa",L"Lock-screen policy unavailable"); return FALSE;
    }
    if (a==A_DRAG && read_dword(CU,CURRENT L"CDP",L"DragTrayEnabled",0xffffffffu)==0xffffffffu &&
        !(p->build>=26200 || (p->build==26100 && p->ubr>=7309))) {
        *reason=tr(L"Chưa hỗ trợ tùy chọn Drag Tray",L"Drag Tray preference unavailable"); return FALSE;
    }
    if (a==A_AI_SERVICE) {
        ServiceState s; DWORD err=service_read(&s);
        if (err) { *reason=err==ERROR_SERVICE_DOES_NOT_EXIST?tr(L"Dịch vụ chưa được cài",L"Service not installed"):tr(L"Không đọc được cấu hình dịch vụ",L"Cannot read service configuration"); return FALSE; }
    }
    if (a==A_STANDBY) {
        PowerState s; if (power_read(&s)) { *reason=tr(L"Không hỗ trợ S0 / tùy chọn mạng",L"S0 / connectivity setting unsupported"); return FALSE; }
    }
    if (a==A_HISTORY && !p->edition) { *reason=tr(L"Cần Pro / Enterprise / Education",L"Requires Pro / Enterprise / Education"); return FALSE; }
    if (a==A_STORE_SEARCH) {
        StoreSecurity saved; DWORD err=store_capture(&saved);
        if (err) { *reason=(err==ERROR_FILE_NOT_FOUND || err==ERROR_PATH_NOT_FOUND)?tr(L"Không có cache gợi ý Store",L"Store suggestion cache absent"):tr(L"Không đọc được quyền cache Store",L"Store cache permissions unavailable"); return FALSE; }
        *reason=tr(L"Chặn cache; sao lưu quyền truy cập",L"Cache workaround; permissions backed up");
    }
    if (a==A_DIAGNOSTICS && p->edition<2) *reason=tr(L"Vẫn còn dữ liệu chẩn đoán bắt buộc",L"Required diagnostic data remains");
    if (a==A_SEARCH && p->edition<2) *reason=tr(L"Một số chính sách web không hỗ trợ",L"Some web policies unavailable on this edition");
    return TRUE;
}
static BOOL op_supported(const RegOp *op,const Platform *p) {
    return p->build>=op->min_build && (!op->max_build || p->build<=op->max_build) && p->edition>=op->edition;
}
static DWORD capture_reg(unsigned id,SavedValue *s) {
    const RegOp *op=&reg_ops[id]; HKEY key=NULL;
    ZeroMemory(s,sizeof(*s)); s->id=id;
    LONG err=RegOpenKeyExW(op->root,op->key,0,KEY_QUERY_VALUE|KEY_WOW64_64KEY,&key);
    if (err==ERROR_FILE_NOT_FOUND || err==ERROR_PATH_NOT_FOUND) return ERROR_SUCCESS;
    if (err) return (DWORD)err;
    s->size=sizeof(s->data);
    err=RegQueryValueExW(key,op->name,NULL,&s->type,s->data,&s->size);
    RegCloseKey(key);
    if (err==ERROR_FILE_NOT_FOUND) { s->size=0; return ERROR_SUCCESS; }
    if (!err) s->exists=1;
    return (DWORD)err;
}
static DWORD write_reg(const SavedValue *s,BOOL restore,const Platform *p) {
    const RegOp *op=&reg_ops[s->id]; HKEY key=NULL;
    if (restore && !s->exists) {
        LONG err=RegOpenKeyExW(op->root,op->key,0,KEY_SET_VALUE|KEY_WOW64_64KEY,&key);
        if (err==ERROR_FILE_NOT_FOUND || err==ERROR_PATH_NOT_FOUND) return ERROR_SUCCESS;
        if (err) return (DWORD)err;
        err=RegDeleteValueW(key,op->name); RegCloseKey(key);
        return err==ERROR_FILE_NOT_FOUND?ERROR_SUCCESS:(DWORD)err;
    }
    LONG err=RegCreateKeyExW(op->root,op->key,0,NULL,0,KEY_SET_VALUE|KEY_WOW64_64KEY,NULL,&key,NULL);
    if (err) return (DWORD)err;
    DWORD value=op->value;
    if (op->action==A_DIAGNOSTICS && !wcscmp(op->name,L"AllowTelemetry")) value=p->edition>=2?0:1;
    if (restore) err=RegSetValueExW(key,op->name,0,s->type,s->data,s->size);
    else if (op->text) err=RegSetValueExW(key,op->name,0,REG_SZ,(const BYTE *)op->text,(DWORD)((wcslen(op->text)+1)*sizeof(wchar_t)));
    else err=RegSetValueExW(key,op->name,0,REG_DWORD,(const BYTE *)&value,sizeof(value));
    RegCloseKey(key); return (DWORD)err;
}
static DWORD snapshot_save(Snapshot *snapshot) {
    wchar_t path[MAX_PATH],temp[MAX_PATH];
    if (!app_data_path(path,ARRAYSIZE(path),L"optimizer-last.snapshot") ||
        !app_data_path(temp,ARRAYSIZE(temp),L"optimizer-pending.snapshot")) return ERROR_PATH_NOT_FOUND;
    HANDLE f=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
    if (f==INVALID_HANDLE_VALUE) return GetLastError();
    DWORD wrote=0,err=ERROR_SUCCESS;
    if (!WriteFile(f,snapshot,sizeof(*snapshot),&wrote,NULL)) err=GetLastError();
    else if (wrote!=sizeof(*snapshot)) err=ERROR_WRITE_FAULT;
    else if (!FlushFileBuffers(f)) err=GetLastError();
    CloseHandle(f);
    if (!err && !MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) err=GetLastError();
    if (!err) app_log(tr(L"Đã lưu bản sao cài đặt: %s", L"Settings snapshot saved: %s"),path);
    return err;
}
static BOOL snapshot_valid(const Snapshot *s) {
    if (s->magic!=0x57555453 || s->version!=SNAP_VERSION || s->count>SNAP_MAX || s->count==0) return FALSE;
    BOOL seen[SNAP_MAX]={0};
    for (unsigned i=0;i<s->count;++i) {
        const SavedValue *v=&s->values[i];
        if (v->id>=SNAP_MAX || seen[v->id] || v->size>SNAP_DATA || v->exists>1) return FALSE;
        if (v->id<REG_COUNT) {
            if (!v->exists && v->size) return FALSE;
            if (v->exists) {
                if (v->type==REG_DWORD && v->size!=sizeof(DWORD)) return FALSE;
                if (v->type==REG_QWORD && v->size!=sizeof(ULONGLONG)) return FALSE;
                if (v->type==REG_SZ || v->type==REG_EXPAND_SZ || v->type==REG_MULTI_SZ) {
                    if (v->size<sizeof(wchar_t) || v->size%sizeof(wchar_t) ||
                        ((const wchar_t *)v->data)[v->size/sizeof(wchar_t)-1]) return FALSE;
                } else if (v->type!=REG_NONE && v->type!=REG_BINARY && v->type!=REG_DWORD && v->type!=REG_QWORD) return FALSE;
            }
        } else if (!v->exists) return FALSE;
        if (v->id==OP_SERVICE && (v->size!=sizeof(ServiceState) || ((const ServiceState *)v->data)->start<SERVICE_AUTO_START || ((const ServiceState *)v->data)->start>SERVICE_DISABLED || ((const ServiceState *)v->data)->delayed>1)) return FALSE;
        if (v->id==OP_POWER && (v->size!=sizeof(PowerState) || ((const PowerState *)v->data)->ac>2 || ((const PowerState *)v->data)->dc>2)) return FALSE;
        if (v->id==OP_STORE_ACL) {
            const StoreSecurity *store=(const StoreSecurity *)v->data;
            if (v->size!=sizeof(StoreSecurity) || store->sd_size<2 || store->sd_size>ARRAYSIZE(store->descriptor) ||
                store->descriptor[store->sd_size-1] || wcsnlen(store->descriptor,store->sd_size)!=store->sd_size-1) return FALSE;
            if (wcsncmp(store->descriptor,L"D:",2)) return FALSE;
            PSECURITY_DESCRIPTOR sd=NULL;
            if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(store->descriptor,SDDL_REVISION_1,&sd,NULL)) return FALSE;
            LocalFree(sd);
        }
        seen[v->id]=TRUE;
    }
    return TRUE;
}
static DWORD snapshot_load(Snapshot *s) {
    wchar_t path[MAX_PATH];
    if (!app_data_path(path,ARRAYSIZE(path),L"optimizer-last.snapshot")) return ERROR_PATH_NOT_FOUND;
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if (f==INVALID_HANDLE_VALUE) return GetLastError();
    DWORD err=ERROR_SUCCESS,got=0;
    LARGE_INTEGER size;
    if (!GetFileSizeEx(f,&size)) err=GetLastError();
    else if (size.QuadPart!=sizeof(*s)) err=ERROR_INVALID_DATA;
    else if (!ReadFile(f,s,sizeof(*s),&got,NULL)) err=GetLastError();
    else if (got!=sizeof(*s) || !snapshot_valid(s)) err=ERROR_INVALID_DATA;
    CloseHandle(f); return err;
}
static unsigned saved_action(const SavedValue *s) {
    return s->id==OP_SERVICE?A_AI_SERVICE:s->id==OP_POWER?A_STANDBY:s->id==OP_STORE_ACL?A_STORE_SEARCH:reg_ops[s->id].action;
}
static DWORD apply_saved(const SavedValue *s,BOOL restore,const Platform *p) {
    if (s->id<REG_COUNT) return write_reg(s,restore,p);
    if (s->id==OP_SERVICE) {
        ServiceState state; memcpy(&state,s->data,sizeof(state));
        /* Already disabled must remain disabled; Manual would otherwise enable it. */
        if (!restore && state.start!=SERVICE_DISABLED) { state.start=SERVICE_DEMAND_START; state.delayed=FALSE; }
        return service_write(&state);
    }
    if (s->id==OP_POWER) {
        PowerState state; memcpy(&state,s->data,sizeof(state));
        if (!restore) state.ac=state.dc=0;
        return power_write(&state);
    }
    if (s->id==OP_STORE_ACL) return store_write((const StoreSecurity *)s->data,restore);
    return ERROR_INVALID_DATA;
}
static void optimizer_work(void *context) {
    OptimizerWindow *w=context; Platform p=platform();
    w->done=w->failed=w->skipped=0;
    if (w->mode==2) {
        for (unsigned i=0;i<w->package_count;++i) if (w->packages[i].selected) {
            if (SUCCEEDED(packages_remove(&w->packages[i]))) ++w->done; else ++w->failed;
        }
        return;
    }
    if (w->mode==1) {
        Snapshot *s=w->restore;
        for (unsigned i=s->count;i>0;--i) {
            DWORD err=apply_saved(&s->values[i-1],TRUE,&p);
            app_log(tr(L"%s: khôi phục %s (mã %lu)", L"%s: restore %s (code %lu)"),err?tr(L"LỖI", L"FAILED"):tr(L"ĐÃ KHÔI PHỤC", L"RESTORED"),tr(actions[saved_action(&s->values[i-1])].vi,actions[saved_action(&s->values[i-1])].en),(unsigned long)err);
            if (err) ++w->failed; else ++w->done;
        }
        free(w->restore); w->restore=NULL;
        return;
    }
    Snapshot *s=calloc(1,sizeof(*s));
    if (!s) { ++w->failed; app_log(tr(L"LỖI: không đủ bộ nhớ; chưa thay đổi cài đặt.", L"FAILED: out of memory; no settings changed.")); return; }
    s->magic=0x57555453; s->version=SNAP_VERSION; s->build=p.build; GetLocalTime(&s->time);
    DWORD err=ERROR_SUCCESS;
    for (unsigned a=0;a<ACTION_COUNT;++a) if (w->selected[a]) {
        const wchar_t *reason;
        if (!action_supported(a,&p,&reason)) { ++w->skipped; app_log(tr(L"KHÔNG HỖ TRỢ: %s: %s", L"UNSUPPORTED: %s: %s"),tr(actions[a].vi,actions[a].en),reason); continue; }
        for (unsigned i=0;i<REG_COUNT;++i) if (reg_ops[i].action==a) {
            if (!op_supported(&reg_ops[i],&p)) { ++w->skipped; app_log(tr(L"BỎ QUA chính sách không hỗ trợ trên phiên bản/build: %s", L"SKIPPED edition/build policy: %s"),reg_ops[i].name); continue; }
            err=capture_reg(i,&s->values[s->count]);
            if (err) break;
            ++s->count;
        }
        if (!err && (a==A_AI_SERVICE || a==A_STANDBY || a==A_STORE_SEARCH)) {
            SavedValue *v=&s->values[s->count]; v->exists=1;
            if (a==A_AI_SERVICE) {
                ServiceState state={0}; err=service_read(&state); v->id=OP_SERVICE; v->size=sizeof(state); memcpy(v->data,&state,sizeof(state));
            } else if (a==A_STANDBY) {
                PowerState state={0}; err=power_read(&state); v->id=OP_POWER; v->size=sizeof(state); memcpy(v->data,&state,sizeof(state));
            } else {
                v->id=OP_STORE_ACL; v->size=sizeof(StoreSecurity); err=store_capture((StoreSecurity *)v->data);
            }
            if (!err) ++s->count;
        }
        if (err) break;
    }
    if (!err && s->count && !snapshot_valid(s)) err=ERROR_INVALID_DATA;
    if (!err && s->count) err=snapshot_save(s);
    if (err) {
        ++w->failed; app_log(tr(L"Không sao lưu được cài đặt hiện tại (%lu). Chưa thay đổi cài đặt hoặc ứng dụng.", L"FAILED to back up current settings (%lu). No settings or packages changed."),(unsigned long)err); free(s); return;
    }
    for (unsigned i=0;i<s->count;++i) {
        const SavedValue *v=&s->values[i]; err=apply_saved(v,FALSE,&p);
        app_log(tr(L"%s: %s%s%s (mã %lu)", L"%s: %s%s%s (code %lu)"),err?tr(L"LỖI", L"FAILED"):tr(L"ĐÃ THIẾT LẬP", L"CONFIGURED"),tr(actions[saved_action(v)].vi,actions[saved_action(v)].en),v->id<REG_COUNT?L" / ":L"",v->id<REG_COUNT?reg_ops[v->id].name:L"",(unsigned long)err);
        if (err) ++w->failed; else ++w->done;
    }
    free(s);
    for (unsigned i=0;i<w->package_count;++i) if (w->packages[i].selected) {
        if (SUCCEEDED(packages_remove(&w->packages[i]))) ++w->done; else ++w->failed;
    }
    app_log(tr(L"Có thể cần đăng xuất hoặc khởi động lại. Phiên bản Windows hoặc chính sách của tổ chức có thể ghi đè thiết lập.", L"Configured settings may require sign-out or restart. Policies can be overridden by Windows edition or organization management."));
}

static void list_column(HWND list,int index,const wchar_t *title,int width) {
    LVCOLUMNW c={0}; c.mask=LVCF_TEXT|LVCF_WIDTH; c.pszText=(wchar_t *)title; c.cx=width;
    ListView_InsertColumn(list,index,&c);
}
static void list_row(HWND list,int index,const wchar_t *label,const wchar_t *status) {
    LVITEMW item={0}; item.mask=LVIF_TEXT; item.iItem=index; item.pszText=(wchar_t *)label;
    ListView_InsertItem(list,&item);
    ListView_SetItemText(list,index,1,(wchar_t *)status);
    ListView_SetCheckState(list,index,FALSE);
}
static void setup_selection_status(OptimizerWindow *w) {
    if (w->bloat || w->has_result) return;
    unsigned count=0;
    for (unsigned a=0;a<ACTION_COUNT;++a)
        if (SendMessageW(w->checks[a],BM_GETCHECK,0,0)==BST_CHECKED) ++count;
    wchar_t text[200];
    StringCchPrintfW(text,ARRAYSIZE(text),tr(L"%u mục đã chọn • Chưa áp dụng thay đổi",L"%u settings selected • No changes applied"),count);
    SetWindowTextW(w->status,text);
}
static void setup_detail(OptimizerWindow *w,unsigned action) {
    if (action>=ACTION_COUNT) return;
    w->detail_action=(int)action;
    wchar_t text[1600];
    StringCchPrintfW(text,ARRAYSIZE(text),L"%s\r\n%s",
        w->reasons[action],tr(details_vi[action],actions[action].detail));
    SetWindowTextW(w->detail,text);
}
static void setup_layout_cards(OptimizerWindow *w) {
    if (!w->viewport) return;
    RECT area; GetClientRect(w->viewport,&area);
    int width=area.right-2, height=area.bottom;
    if (width<1 || height<1) return;
    int columns=width>=1080?3:width>=660?2:1;
    int card_width=(width-(columns-1)*16)/columns;
    int bottoms[3]={0,0,0};
    wchar_t search[160]; GetDlgItemTextW(w->window,ID_SEARCH,search,ARRAYSIZE(search));
    HDC dc=GetDC(w->viewport); HGDIOBJ font=SelectObject(dc,g_font);
    for (unsigned g=0;g<ARRAYSIZE(setup_groups);++g) {
        const SetupGroup *group=&setup_groups[g];
        unsigned count=0;
        for (unsigned i=0;i<group->count;++i) {
            unsigned a=group->actions[i];
            w->visible[a]=!search[0] || StrStrIW(actions[a].en,search) || StrStrIW(actions[a].vi,search) ||
                StrStrIW(group->en,search) || StrStrIW(group->vi,search);
            if (w->visible[a]) ++count;
        }
        SetRectEmpty(&w->cards[g]);
        if (!count) continue;
        int column=0;
        for (int c=1;c<columns;++c) if (bottoms[c]<bottoms[column]) column=c;
        int x=column*(card_width+16), y=bottoms[column], row_y=y+48;
        for (unsigned i=0;i<group->count;++i) {
            unsigned a=group->actions[i]; if (!w->visible[a]) continue;
            RECT measure={0,0,card_width-66,0};
            DrawTextW(dc,tr(actions[a].vi,actions[a].en),-1,&measure,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);
            int row_height=max(38,measure.bottom+16);
            SetRect(&w->rows[a],x+16,row_y,x+card_width-16,row_y+row_height);
            row_y+=row_height+4;
        }
        SetRect(&w->cards[g],x,y,x+card_width,row_y+12);
        bottoms[column]=row_y+28;
    }
    SelectObject(dc,font); ReleaseDC(w->viewport,dc);
    w->content_height=0;
    for (int c=0;c<columns;++c) w->content_height=max(w->content_height,bottoms[c]);
    w->scroll=max(0,min(w->scroll,max(0,w->content_height-height)));
    SCROLLINFO scroll={sizeof(scroll),SIF_RANGE|SIF_PAGE|SIF_POS|SIF_DISABLENOSCROLL,0,max(0,w->content_height-1),(UINT)height,w->scroll,0};
    SetScrollInfo(w->viewport,SB_VERT,&scroll,TRUE);
    for (unsigned g=0;g<ARRAYSIZE(setup_groups);++g) {
        RECT card=w->cards[g];
        ShowWindow(w->group_labels[g],IsRectEmpty(&card)?SW_HIDE:SW_SHOW);
        if (!IsRectEmpty(&card)) theme_move(w->group_labels[g],card.left+16,card.top+16-w->scroll,card.right-card.left-32,26);
    }
    for (unsigned a=0;a<ACTION_COUNT;++a) {
        ShowWindow(w->checks[a],w->visible[a]?SW_SHOW:SW_HIDE);
        if (w->visible[a]) {
            RECT row=w->rows[a];
            theme_move(w->checks[a],row.left,row.top-w->scroll,row.right-row.left,row.bottom-row.top);
        }
    }
    RedrawWindow(w->viewport,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}
static void setup_scroll(OptimizerWindow *w,int position) {
    if (position==w->scroll) return;
    w->scroll=position; setup_layout_cards(w);
}
static LRESULT CALLBACK setup_check_proc(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data) {
    (void)id;
    if (message==WM_SETFOCUS || message==WM_MOUSEMOVE)
        SendMessageW(GetParent(window),WM_SETUP_FOCUS,(WPARAM)data,message==WM_SETFOCUS);
    if (message==WM_MOUSEWHEEL) return SendMessageW(GetParent(window),message,wp,lp);
    return DefSubclassProc(window,message,wp,lp);
}
static LRESULT CALLBACK setup_view_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    OptimizerWindow *w=(OptimizerWindow *)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
    switch (message) {
    case WM_CREATE:
        w=((CREATESTRUCTW *)lp)->lpCreateParams;
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,(LONG_PTR)w); theme_attach(hwnd); return 0;
    case WM_COMMAND: return SendMessageW(GetParent(hwnd),message,wp,lp);
    case WM_SETUP_FOCUS:
        if (w && wp<ACTION_COUNT) {
            if (w->detail_action!=(int)wp) setup_detail(w,(unsigned)wp);
            if (lp) {
                RECT area; GetClientRect(hwnd,&area);
                RECT row=w->rows[wp];
                if (row.top<w->scroll) setup_scroll(w,row.top-12);
                else if (row.bottom>w->scroll+area.bottom) setup_scroll(w,row.bottom-area.bottom+12);
            }
        } return 0;
    case WM_MOUSEWHEEL:
        if (w) setup_scroll(w,w->scroll-GET_WHEEL_DELTA_WPARAM(wp)*60/WHEEL_DELTA);
        return 0;
    case WM_MOUSEMOVE:
        if (w) {
            POINT point={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)+w->scroll};
            for (unsigned a=0;a<ACTION_COUNT;++a)
                if (w->visible[a] && PtInRect(&w->rows[a],point) && w->detail_action!=(int)a) setup_detail(w,a);
        } return 0;
    case WM_VSCROLL:
        if (w) {
            SCROLLINFO info={0}; info.cbSize=sizeof(info); info.fMask=SIF_ALL; GetScrollInfo(hwnd,SB_VERT,&info);
            int position=w->scroll;
            switch (LOWORD(wp)) {
            case SB_LINEUP: position-=42; break;
            case SB_LINEDOWN: position+=42; break;
            case SB_PAGEUP: position-=(int)info.nPage; break;
            case SB_PAGEDOWN: position+=(int)info.nPage; break;
            case SB_THUMBTRACK: case SB_THUMBPOSITION: position=info.nTrackPos; break;
            case SB_TOP: position=0; break;
            case SB_BOTTOM: position=info.nMax; break;
            }
            setup_scroll(w,position);
        } return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint; HDC dc=BeginPaint(hwnd,&paint); RECT area; GetClientRect(hwnd,&area);
        FillRect(dc,&area,g_theme.background_brush);
        BOOL any=FALSE;
        if (w) for (unsigned g=0;g<ARRAYSIZE(setup_groups);++g) {
            RECT card=w->cards[g]; if (IsRectEmpty(&card)) continue;
            any=TRUE; OffsetRect(&card,0,-w->scroll); theme_card(dc,&card);
        }
        if (!any) {
            HGDIOBJ font=SelectObject(dc,g_font); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,g_theme.muted);
            area.top+=36;
            DrawTextW(dc,tr(L"Không tìm thấy cài đặt phù hợp.",L"No matching settings."),-1,&area,DT_CENTER|DT_TOP|DT_NOPREFIX);
            SelectObject(dc,font);
        }
        EndPaint(hwnd,&paint); return 0;
    }
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
static void optimizer_layout(OptimizerWindow *w) {
    RECT area; GetClientRect(w->window,&area); int width=area.right,height=area.bottom;
    theme_move(GetDlgItem(w->window,ID_HEADING),24,20,width-224,36);
    theme_move(GetDlgItem(w->window,ID_ADVANCED),width-178,20,154,34);
    SetDlgItemTextW(w->window,ID_ADVANCED,w->advanced?tr(L"Thu gọn",L"Show less"):tr(L"Nâng cao",L"Advanced"));
    ShowWindow(w->detail,w->advanced?SW_SHOW:SW_HIDE);
    theme_move(GetDlgItem(w->window,ID_SUBHEADING),24,60,width-48,26);
    theme_move(GetDlgItem(w->window,ID_NONE),24,w->bloat?104:214,150,36);
    theme_move(GetDlgItem(w->window,ID_REFRESH),184,w->bloat?104:214,100,36);
    if (!w->bloat) {
        theme_move(GetDlgItem(w->window,ID_RECOVERY_HEADING),40,110,width-80,24);
        int half=(width-96)/2;
        theme_move(GetDlgItem(w->window,ID_REG_BACKUP),40,136,half,42);
        theme_move(GetDlgItem(w->window,ID_RESTORE_POINT),56+half,136,half,42);
        theme_move(GetDlgItem(w->window,ID_RECOVERY_NOTE),40,178,width-80,22);
        theme_move(GetDlgItem(w->window,ID_SEARCH),width-324,214,300,36);
        theme_move(w->viewport,24,266,width-48,height-(w->advanced?426:354));
        theme_move(GetDlgItem(w->window,ID_RESTORE),24,height-52,190,36);
        theme_move(GetDlgItem(w->window,ID_BLOAT),224,height-52,180,36);
        setup_layout_cards(w);
    } else {
        theme_report_layout(w->list,24,154,width-48,height-(w->advanced?316:244));
        int list_width=width-48-GetSystemMetrics(SM_CXVSCROLL)-4;
        ListView_SetColumnWidth(w->list,0,w->advanced?list_width/2:list_width);
        ListView_SetColumnWidth(w->list,1,w->advanced?list_width-list_width/2:0);
    }
    theme_move(w->detail,24,height-146,width-48,60);
    theme_move(w->status,24,height-78,width-48,22);
    theme_move(GetDlgItem(w->window,ID_APPLY),width-228,height-52,204,36);
    RedrawWindow(w->window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}
static void setup_refresh_support(OptimizerWindow *w) {
    Platform p=platform();
    for (unsigned a=0;a<ACTION_COUNT;++a) {
        const wchar_t *reason;
        w->supported[a]=action_supported(a,&p,&reason);
        StringCchCopyW(w->reasons[a],ARRAYSIZE(w->reasons[a]),reason);
        EnableWindow(w->checks[a],w->supported[a]);
        if (!w->supported[a]) SendMessageW(w->checks[a],BM_SETCHECK,BST_UNCHECKED,0);
    }
}
static BOOL setup_confirmation(OptimizerWindow *w,const wchar_t *details) {
    if (w->bloat) return app_confirm(w->window,details);
    BOOL registry=IsDlgButtonChecked(w->window,ID_REG_BACKUP)==BST_CHECKED;
    BOOL restore=IsDlgButtonChecked(w->window,ID_RESTORE_POINT)==BST_CHECKED;
    size_t capacity=wcslen(details)+1024;
    wchar_t *review=calloc(capacity,sizeof(*review)); if (!review) return FALSE;
    StringCchPrintfW(review,capacity,tr(
        L"%s\r\n\r\nBẢO VỆ TRƯỚC KHI TỐI ƯU\r\nTạo bản sao Registry (Khuyên dùng): %s\r\nTạo điểm khôi phục hệ thống (Khuyên dùng): %s\r\n\r\nChỉ chạy các bước đã chọn sau khi bạn đồng ý.",
        L"%s\r\n\r\nRECOVERY BEFORE OPTIMIZATION\r\nCreate a registry backup (Recommended): %s\r\nCreate a system restore point (Recommended): %s\r\n\r\nSelected steps run only after you agree."),
        details,registry?tr(L"Có",L"Yes"):tr(L"Không",L"No"),restore?tr(L"Có",L"Yes"):tr(L"Không",L"No"));
    BOOL accepted=app_confirm_setup(w->window,review,registry,restore);
    free(review); return accepted;
}
static void refresh_list(OptimizerWindow *w) {
    w->has_result=FALSE;
    if (w->bloat) ListView_DeleteAllItems(w->list);
    if (w->bloat) {
        DWORD err=app_is_smoke_test()?ERROR_SUCCESS:packages_enumerate(w->packages,ARRAYSIZE(w->packages),&w->package_count,NULL);
        if (err) { app_error(w->window,tr(L"Đọc danh sách ứng dụng tùy chọn", L"Read optional packages"),err); return; }
        for (unsigned i=0;i<w->package_count;++i) list_row(w->list,(int)i,w->packages[i].display,w->packages[i].full_name);
        SetWindowTextW(w->status,w->package_count?tr(L"Chưa chọn ứng dụng nào. Chỉ gỡ cho tài khoản hiện tại.",L"No apps selected. Removal affects the current account only."):tr(L"Không tìm thấy ứng dụng tùy chọn trong danh sách.",L"No allowlisted optional apps are installed for this account."));
    } else {
        setup_refresh_support(w);
        setup_selection_status(w);
    }
}
static void begin_apply(OptimizerWindow *w) {
    if (app_busy(NULL)) return;
    wchar_t *confirmation=calloc(65536,sizeof(wchar_t));
    if (!confirmation) return;
    StringCchCopyW(confirmation,65536,tr(L"Áp dụng CHÍNH XÁC các mục đã chọn dưới đây?\r\n\r\n",L"Apply EXACTLY the selected items below?\r\n\r\n"));
    unsigned selected=0; w->package_count=w->bloat?w->package_count:0;
    for (unsigned i=0;i<(w->bloat?w->package_count:ACTION_COUNT);++i) {
        BOOL checked=w->bloat?ListView_GetCheckState(w->list,(int)i):
            SendMessageW(w->checks[i],BM_GETCHECK,0,0)==BST_CHECKED;
        if (w->bloat) w->packages[i].selected=checked; else w->selected[i]=checked;
        if (!checked) continue;
        ++selected;
        StringCchCatW(confirmation,65536,L"• ");
        StringCchCatW(confirmation,65536,w->bloat?w->packages[i].display:tr(actions[i].vi,actions[i].en));
        StringCchCatW(confirmation,65536,L"\r\n");
        if (w->bloat) {
            StringCchCatW(confirmation,65536,w->packages[i].full_name);
            StringCchCatW(confirmation,65536,L"\r\n");
        }
    }
    if (!selected) { free(confirmation); MessageBoxW(w->window,tr(L"Hãy chọn ít nhất một mục.",L"Select at least one item."),L"Win AIO Optimize",MB_OK|MB_ICONINFORMATION); return; }
    if (!w->bloat) {
        const unsigned apps[]={A_SEARCH,A_COPILOT};
        const wchar_t *families[]={L"Microsoft.549981C3F5F10_8wekyb3d8bbwe",L"Microsoft.Copilot_8wekyb3d8bbwe"};
        for (unsigned i=0;i<ARRAYSIZE(apps);++i) if (w->selected[apps[i]]) {
            unsigned count=0;
            DWORD err=packages_enumerate(w->packages+w->package_count,PACKAGE_ITEMS_MAX-w->package_count,&count,families[i]);
            if (err) { free(confirmation); app_error(w->window,tr(L"Đọc danh sách gói trước khi xác nhận", L"Read packages before confirmation"),err); return; }
            for (unsigned j=0;j<count;++j) {
                PackageItem *item=&w->packages[w->package_count+j]; item->selected=TRUE;
                StringCchCatW(confirmation,65536,tr(L"Gỡ: ",L"Remove: ")); StringCchCatW(confirmation,65536,item->full_name); StringCchCatW(confirmation,65536,L"\r\n");
            }
            w->package_count+=count;
        }
        StringCchCatW(confirmation,65536,tr(L"\r\nLưu trạng thái cài đặt trước khi sửa, thay thế bản sao lưu lần trước. Khôi phục không cài lại ứng dụng hoặc dữ liệu đã xóa. Có thể cần đăng xuất/khởi động lại; không tự khởi động lại. Một số chính sách bị giới hạn theo bản Windows.\r\n",L"\r\nSave prior settings before changes, replacing the previous snapshot. Restore does not reinstall removed apps or recover deleted data. Sign-out/restart may be needed; no automatic restart. Some policies depend on Windows edition.\r\n"));
        if (w->selected[A_RECALL]) StringCchCatW(confirmation,65536,tr(L"\r\nRECALL: Windows sẽ XÓA ảnh chụp đã lưu; không thể hoàn tác dữ liệu này.\r\n",L"\r\nRECALL: Windows will DELETE saved Recall snapshots; this data cannot be restored.\r\n"));
        if (w->selected[A_STANDBY]) StringCchCatW(confirmation,65536,tr(L"Mạng khi ngủ sẽ tắt trên pin và nguồn AC; ảnh hưởng thông báo/truy cập từ xa.\r\n",L"Sleep networking will be disabled on battery and AC; notifications/remote access may be affected.\r\n"));
        if (w->selected[A_STORE_SEARCH]) StringCchCatW(confirmation,65536,tr(L"Store: chặn quyền đọc/ghi dữ liệu cache store.db cho tài khoản hiện tại; có thể ảnh hưởng gợi ý trong Store. Quyền gốc được sao lưu; không xóa file.\r\n",L"Store: block data reads/writes to the current user's store.db cache; Store recommendations may be affected. Original permissions are saved; no file is deleted.\r\n"));
    }
    if (w->bloat || w->package_count) StringCchCatW(confirmation,65536,tr(L"\r\nGỡ ứng dụng chỉ cho tài khoản hiện tại. DỮ LIỆU ỨNG DỤNG CÓ THỂ BỊ XÓA. Muốn dùng lại, cài từ Microsoft Store; bản sao cài đặt không khôi phục dữ liệu.\r\n",L"\r\nRemove apps for the current account only. APP DATA MAY BE DELETED. Reinstall from Microsoft Store if needed; the settings snapshot cannot restore app data.\r\n"));
    BOOL accepted=setup_confirmation(w,confirmation); free(confirmation);
    if (!accepted) return;
    w->mode=w->bloat?2:0;
    SetWindowTextW(w->status,tr(L"Đang xử lý…",L"Working…"));
    app_start_job(w->window,optimizer_work,w);
}
static void begin_restore(OptimizerWindow *w) {
    if (app_busy(NULL)) return;
    Snapshot *s=calloc(1,sizeof(*s)); if (!s) return;
    DWORD err=snapshot_load(s);
    if (err) { free(s); app_error(w->window,tr(L"Đọc bản sao cài đặt gần nhất", L"Load last settings snapshot"),err); return; }
    wchar_t text[8192],line[256]; BOOL listed[ACTION_COUNT]={0};
    StringCchPrintfW(text,ARRAYSIZE(text),tr(L"Khôi phục cài đặt trước lần áp dụng %04u-%02u-%02u %02u:%02u?\r\n\r\n",L"Restore settings saved before the run on %04u-%02u-%02u %02u:%02u?\r\n\r\n"),s->time.wYear,s->time.wMonth,s->time.wDay,s->time.wHour,s->time.wMinute);
    for (unsigned i=0;i<s->count;++i) {
        unsigned a=saved_action(&s->values[i]); if (listed[a]) continue; listed[a]=TRUE;
        StringCchPrintfW(line,ARRAYSIZE(line),L"• %s\r\n",tr(actions[a].vi,actions[a].en)); StringCchCatW(text,ARRAYSIZE(text),line);
    }
    StringCchCatW(text,ARRAYSIZE(text),tr(L"\r\nThay thế các giá trị hiện tại bằng trạng thái đã lưu. Không khôi phục ứng dụng, dữ liệu ứng dụng hoặc ảnh chụp Recall. Có thể cần khởi động lại.",L"\r\nReplaces current values with the saved state. Does not restore apps, app data, or Recall snapshots. A restart may be needed."));
    if (!setup_confirmation(w,text)) { free(s); return; }
    w->restore=s; w->mode=1;
    if (!app_start_job(w->window,optimizer_work,w)) { free(s); w->restore=NULL; }
}
static void translate_optimizer(OptimizerWindow *w) {
    SetWindowTextW(w->window,w->bloat?tr(L"Ứng dụng tùy chọn — Win AIO Optimize",L"Optional apps — Win AIO Optimize"):L"Windows Setup — Win AIO Optimize");
    SetDlgItemTextW(w->window,ID_HEADING,w->bloat?tr(L"Ứng dụng tùy chọn",L"Optional apps"):tr(L"Tối ưu Windows",L"Windows Setup"));
    SetDlgItemTextW(w->window,ID_SUBHEADING,w->bloat?
        tr(L"Chọn chính xác ứng dụng cần gỡ. Dữ liệu ứng dụng có thể bị xóa.",L"Choose the exact apps to remove. App data may be deleted."):
        tr(L"Chọn cài đặt muốn thay đổi. Bạn sẽ được xem lại trước khi áp dụng.",L"Select settings to change. Review them before applying."));
    SetDlgItemTextW(w->window,ID_APPLY,w->bloat?tr(L"Gỡ mục đã chọn…",L"Remove selected…"):tr(L"Xem lại & áp dụng…",L"Review & apply…"));
    SetDlgItemTextW(w->window,ID_RESTORE,tr(L"Khôi phục cài đặt…",L"Restore last settings…"));
    SetDlgItemTextW(w->window,ID_BLOAT,tr(L"Ứng dụng tùy chọn…",L"Optional apps…"));
    SetDlgItemTextW(w->window,ID_NONE,tr(L"Bỏ chọn tất cả",L"Clear selection"));
    SetDlgItemTextW(w->window,ID_REFRESH,tr(L"Làm mới",L"Refresh"));
    if (w->bloat) {
        const wchar_t *columns[]={tr(L"Ứng dụng",L"App"),tr(L"Định danh gói",L"Package identity")};
        for (int i=0;i<2;++i) {
            LVCOLUMNW column={0}; column.mask=LVCF_TEXT; column.pszText=(wchar_t *)columns[i]; ListView_SetColumn(w->list,i,&column);
        }
        for (unsigned i=0;i<w->package_count;++i) {
            StringCchCopyW(w->packages[i].display,ARRAYSIZE(w->packages[i].display),packages_display_name(w->packages[i].family));
            ListView_SetItemText(w->list,(int)i,0,w->packages[i].display);
        }
    } else {
        SetDlgItemTextW(w->window,ID_RECOVERY_HEADING,tr(L"Bảo vệ trước khi tối ưu",L"Recovery before optimization"));
        SetDlgItemTextW(w->window,ID_REG_BACKUP,tr(L"Tạo bản sao Registry (Khuyên dùng)",L"Create a registry backup (Recommended)"));
        SetDlgItemTextW(w->window,ID_RESTORE_POINT,tr(L"Tạo điểm khôi phục hệ thống (Khuyên dùng)",L"Create a system restore point (Recommended)"));
        SetDlgItemTextW(w->window,ID_RECOVERY_NOTE,tr(
            L"Chỉ tạo khi xác nhận thay đổi tại đây. Điểm khôi phục cần bật System Protection.",
            L"Created only when you confirm changes here. Restore points require System Protection."));
        SendDlgItemMessageW(w->window,ID_SEARCH,EM_SETCUEBANNER,FALSE,(LPARAM)tr(L"Tìm cài đặt…",L"Search settings…"));
        for (unsigned g=0;g<ARRAYSIZE(setup_groups);++g) SetWindowTextW(w->group_labels[g],tr(setup_groups[g].vi,setup_groups[g].en));
        for (unsigned a=0;a<ACTION_COUNT;++a) SetWindowTextW(w->checks[a],tr(actions[a].vi,actions[a].en));
        setup_refresh_support(w);
        if (w->detail_action>=0) setup_detail(w,(unsigned)w->detail_action);
        else SetWindowTextW(w->detail,tr(L"Cài đặt không hỗ trợ sẽ bị làm mờ. Mọi lựa chọn tối ưu mặc định đều tắt; chỉ áp dụng sau khi bạn xác nhận.",
            L"Unavailable settings are dimmed. All optimization choices start unchecked; changes require your confirmation."));
    }
    if (w->has_result) {
        wchar_t text[400];
        StringCchPrintfW(text,ARRAYSIZE(text),tr(L"Xong: %u thành công, %u lỗi, %u bỏ qua. Xem nhật ký để biết chi tiết.",L"Finished: %u succeeded, %u failed, %u skipped. See the log for details."),w->done,w->failed,w->skipped);
        SetWindowTextW(w->status,text);
    } else if (w->bloat) SetWindowTextW(w->status,w->package_count?
        tr(L"Chỉ gỡ ứng dụng đã chọn cho tài khoản hiện tại.",L"Only selected apps will be removed for the current account."):
        tr(L"Không tìm thấy ứng dụng tùy chọn trong danh sách.",L"No allowlisted optional apps are installed for this account."));
    else setup_selection_status(w);
    optimizer_layout(w);
}
static LRESULT CALLBACK optimizer_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    OptimizerWindow *w=(OptimizerWindow *)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
    switch (message) {
    case WM_CREATE: {
        w=((CREATESTRUCTW *)lp)->lpCreateParams; w->window=hwnd; w->detail_action=-1;
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,(LONG_PTR)w); theme_attach(hwnd);
        w->heading_font=CreateFontW(-28,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        w->section_font=CreateFontW(-16,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        HWND heading=app_control(hwnd,L"STATIC",L"",0,0,0,0,0,ID_HEADING);
        SendMessageW(heading,WM_SETFONT,(WPARAM)w->heading_font,TRUE);
        app_control(hwnd,L"STATIC",L"",0,0,0,0,0,ID_SUBHEADING);
        app_control(hwnd,L"BUTTON",L"",WS_TABSTOP,0,0,0,0,ID_ADVANCED);
        if (w->bloat) {
            w->list=app_control(hwnd,WC_LISTVIEWW,L"",LVS_REPORT|LVS_SHOWSELALWAYS|WS_TABSTOP,0,0,0,0,ID_LIST);
            if (!w->list) return -1;
            ListView_SetExtendedListViewStyle(w->list,LVS_EX_CHECKBOXES|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
            list_column(w->list,0,L"",420); list_column(w->list,1,L"",420);
        } else {
            HWND recovery=app_control(hwnd,L"STATIC",L"",0,0,0,0,0,ID_RECOVERY_HEADING);
            SendMessageW(recovery,WM_SETFONT,(WPARAM)w->section_font,TRUE); SetPropW(recovery,L"WAIOCard",(HANDLE)1);
            int options[]={ID_REG_BACKUP,ID_RESTORE_POINT};
            for (unsigned i=0;i<ARRAYSIZE(options);++i) {
                HWND control=app_control(hwnd,L"BUTTON",L"",BS_AUTOCHECKBOX|BS_MULTILINE|WS_TABSTOP,0,0,0,0,options[i]);
                if (!control) return -1;
                SetPropW(control,L"WAIOCard",(HANDLE)1); SendMessageW(control,BM_SETCHECK,BST_CHECKED,0);
            }
            SetPropW(app_control(hwnd,L"STATIC",L"",0,0,0,0,0,ID_RECOVERY_NOTE),L"WAIOCard",(HANDLE)1);
            HWND search=app_control(hwnd,L"EDIT",L"",ES_AUTOHSCROLL|WS_TABSTOP,0,0,0,0,ID_SEARCH);
            SendMessageW(search,EM_SETLIMITTEXT,159,0);
            WNDCLASSW cls={0}; cls.hInstance=g_instance; cls.lpfnWndProc=setup_view_proc;
            cls.lpszClassName=L"WAIO.SetupCards"; cls.hCursor=LoadCursorW(NULL,IDC_ARROW); RegisterClassW(&cls);
            w->viewport=CreateWindowExW(WS_EX_CONTROLPARENT,cls.lpszClassName,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_VSCROLL,
                0,0,1,1,hwnd,(HMENU)(INT_PTR)ID_VIEWPORT,g_instance,w);
            if (!w->viewport) return -1;
            for (unsigned g=0;g<ARRAYSIZE(setup_groups);++g) {
                w->group_labels[g]=app_control(w->viewport,L"STATIC",L"",0,0,0,0,0,0);
                SetPropW(w->group_labels[g],L"WAIOCard",(HANDLE)1); SendMessageW(w->group_labels[g],WM_SETFONT,(WPARAM)w->section_font,TRUE);
            }
            for (unsigned a=0;a<ACTION_COUNT;++a) {
                HWND control=app_control(w->viewport,L"BUTTON",L"",BS_AUTOCHECKBOX|BS_MULTILINE|WS_TABSTOP,0,0,0,0,ID_ACTION_BASE+(int)a);
                if (!control) return -1;
                w->checks[a]=control; SetPropW(control,L"WAIOCard",(HANDLE)1);
                SetWindowSubclass(control,setup_check_proc,2,a);
            }
            app_control(hwnd,L"BUTTON",L"",WS_TABSTOP,0,0,0,0,ID_RESTORE);
            app_control(hwnd,L"BUTTON",L"",WS_TABSTOP,0,0,0,0,ID_BLOAT);
        }
        w->detail=app_control(hwnd,L"EDIT",L"",ES_MULTILINE|ES_READONLY|WS_VSCROLL,0,0,0,0,ID_DETAIL);
        w->status=app_control(hwnd,L"STATIC",L"",0,0,0,0,0,ID_STATUS);
        HWND apply=app_control(hwnd,L"BUTTON",L"",WS_TABSTOP,0,0,0,0,ID_APPLY);
        SetPropW(apply,L"WAIOPrimary",(HANDLE)1);
        app_control(hwnd,L"BUTTON",L"",WS_TABSTOP,0,0,0,0,ID_NONE);
        app_control(hwnd,L"BUTTON",L"",WS_TABSTOP,0,0,0,0,ID_REFRESH);
        refresh_list(w); translate_optimizer(w); w->initialized=TRUE; return 0;
    }
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)lp)->ptMinTrackSize.x=860; ((MINMAXINFO *)lp)->ptMinTrackSize.y=700; return 0;
    case WM_SIZE:
        if (w && w->initialized && wp!=SIZE_MINIMIZED) optimizer_layout(w);
        return 0;
    case WM_COMMAND:
        if (!w || app_busy(NULL)) return 0;
        if (LOWORD(wp)>=ID_ACTION_BASE && LOWORD(wp)<ID_ACTION_BASE+ACTION_COUNT) {
            unsigned a=LOWORD(wp)-ID_ACTION_BASE;
            if (!w->supported[a]) SendMessageW(w->checks[a],BM_SETCHECK,BST_UNCHECKED,0);
            w->has_result=FALSE; setup_detail(w,a); setup_selection_status(w); return 0;
        }
        switch (LOWORD(wp)) {
        case ID_ADVANCED: w->advanced=!w->advanced; optimizer_layout(w); break;
        case ID_APPLY: begin_apply(w); break;
        case ID_RESTORE: begin_restore(w); break;
        case ID_BLOAT: bloatware_show(hwnd); break;
        case ID_REFRESH: refresh_list(w); translate_optimizer(w); break;
        case ID_SEARCH:
            if (HIWORD(wp)==EN_CHANGE && w->initialized) { w->scroll=0; setup_layout_cards(w); }
            break;
        case ID_NONE:
            if (w->bloat) {
                for (int i=0;i<ListView_GetItemCount(w->list);++i) { ListView_SetCheckState(w->list,i,FALSE); }
            } else {
                for (unsigned a=0;a<ACTION_COUNT;++a) SendMessageW(w->checks[a],BM_SETCHECK,BST_UNCHECKED,0);
            }
            w->has_result=FALSE; setup_selection_status(w); break;
        } return 0;
    case WM_NOTIFY:
        if (w && w->bloat && ((NMHDR *)lp)->idFrom==ID_LIST && ((NMHDR *)lp)->code==LVN_ITEMCHANGED) {
            NMLISTVIEW *change=(NMLISTVIEW *)lp;
            if (change->iItem>=0 && (unsigned)change->iItem<w->package_count && (change->uNewState&LVIS_SELECTED))
                SetWindowTextW(w->detail,w->packages[change->iItem].full_name);
        } break;
    case WM_APP_JOB_DONE: {
        if (!w) return 0;
        app_finish_job(hwnd);
        if (w->bloat) refresh_list(w);
        w->has_result=TRUE; translate_optimizer(w);
        wchar_t text[400]; GetWindowTextW(w->status,text,ARRAYSIZE(text)); app_log(L"%s",text);
        return 0;
    }
    case WM_APP_LANGUAGE:
        if (w) translate_optimizer(w);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint; HDC dc=BeginPaint(hwnd,&paint); RECT area; GetClientRect(hwnd,&area);
        FillRect(dc,&area,g_theme.background_brush);
        if (w && !w->bloat) { RECT card={24,98,area.right-24,202}; theme_card(dc,&card); }
        EndPaint(hwnd,&paint); return 0;
    }
    case WM_CLOSE:
        if (app_busy(NULL) || !IsWindowEnabled(hwnd)) return 0;
        app_home(); return 0;
    case WM_NCDESTROY:
        app_page_destroyed(hwnd);
        if (w) {
            DeleteObject(w->heading_font); DeleteObject(w->section_font); free(w->restore);
            if (w->initialized) free(w);
        }
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,0); break;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
static HWND show_optimizer(HWND owner,BOOL bloat) {
    static ATOM cls=0;
    static HWND windows[2]={NULL,NULL};
    if (IsWindow(windows[bloat?1:0])) {
        app_show_page(windows[bloat?1:0]); return windows[bloat?1:0];
    }
    if (!cls) {
        WNDCLASSW wc={0}; wc.lpfnWndProc=optimizer_proc; wc.hInstance=g_instance; wc.lpszClassName=L"WAIONativeOptimizer";
        wc.hCursor=LoadCursorW(NULL,IDC_ARROW); wc.hIcon=LoadIconW(g_instance,MAKEINTRESOURCEW(1)); cls=RegisterClassW(&wc);
    }
    OptimizerWindow *w=calloc(1,sizeof(*w)); if (!w) return NULL; w->bloat=bloat;
    HWND hwnd=CreateWindowExW(WS_EX_CONTROLPARENT,L"WAIONativeOptimizer",L"Windows Setup",WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
        0,0,bloat?1000:1280,880,app_main_window(),NULL,g_instance,w);
    if (!hwnd) { free(w); return NULL; }
    windows[bloat?1:0]=hwnd;
    SetPropW(hwnd,L"WAIONav",(HANDLE)(INT_PTR)(bloat?103:101));
    app_show_page(hwnd);
    (void)owner;
    return hwnd;
}
void optimizer_show(HWND owner) { show_optimizer(owner,FALSE); }
void bloatware_show(HWND owner) { show_optimizer(owner,TRUE); }
void optimizer_preview(HWND owner) {
    if (!app_is_smoke_test()) return;
    HWND window=show_optimizer(owner,FALSE);
    if (window) ShowWindow(window,SW_SHOWNOACTIVATE);
}
BOOL optimizer_ui_test(HWND owner) {
    if (!app_is_smoke_test()) return FALSE;
    BOOL original=g_vi,ok=TRUE;
    for (int bloat=0;bloat<2;++bloat) {
        HWND window=show_optimizer(owner,bloat!=0);
        if (!window) { ok=FALSE; break; }
        OptimizerWindow *w=(OptimizerWindow *)GetWindowLongPtrW(window,GWLP_USERDATA);
        if (bloat) {
            ListView_DeleteAllItems(w->list); w->package_count=1;
            StringCchCopyW(w->packages[0].family,ARRAYSIZE(w->packages[0].family),L"Microsoft.GetHelp_8wekyb3d8bbwe");
            list_row(w->list,0,L"Get Help",L"test package identity");
            ListView_SetCheckState(w->list,0,TRUE);
            ListView_SetItemState(w->list,0,LVIS_SELECTED,LVIS_SELECTED);
        } else {
            for (unsigned a=0;a<ACTION_COUNT;++a)
                if (SendMessageW(w->checks[a],BM_GETCHECK,0,0)!=BST_UNCHECKED) ok=FALSE;
            if (IsDlgButtonChecked(window,ID_REG_BACKUP)!=BST_CHECKED || IsDlgButtonChecked(window,ID_RESTORE_POINT)!=BST_CHECKED) ok=FALSE;
            SendMessageW(w->checks[A_EXTENSIONS],BM_CLICK,0,0);
            CheckDlgButton(window,ID_REG_BACKUP,BST_UNCHECKED);
            SetDlgItemTextW(window,ID_SEARCH,L"extensions");
            for (unsigned a=0;a<ACTION_COUNT;++a)
                if (w->visible[a]!=(a==A_EXTENSIONS)) ok=FALSE;
        }
        if (w->advanced || (GetWindowLongPtrW(w->detail,GWL_STYLE)&WS_VISIBLE)) ok=FALSE;
        SendMessageW(window,WM_COMMAND,ID_ADVANCED,0);
        if (!w->advanced || !(GetWindowLongPtrW(w->detail,GWL_STYLE)&WS_VISIBLE)) ok=FALSE;
        SendMessageW(window,WM_COMMAND,ID_ADVANCED,0);
        for (int iteration=0;iteration<6;++iteration) {
            wchar_t text[1600];
            SendDlgItemMessageW(owner,111,CB_SETCURSEL,(iteration%2)?0:1,0);
            SendMessageW(owner,WM_COMMAND,MAKEWPARAM(111,CBN_SELCHANGE),(LPARAM)GetDlgItem(owner,111));
            if (bloat) {
                if (!ListView_GetCheckState(w->list,0) || ListView_GetNextItem(w->list,-1,LVNI_SELECTED)!=0) ok=FALSE;
                ListView_GetItemText(w->list,0,0,text,ARRAYSIZE(text));
                if (wcscmp(text,tr(L"Get Help (trợ giúp)",L"Get Help"))) ok=FALSE;
            } else {
                if (SendMessageW(w->checks[A_EXTENSIONS],BM_GETCHECK,0,0)!=BST_CHECKED ||
                    IsDlgButtonChecked(window,ID_REG_BACKUP)!=BST_UNCHECKED ||
                    IsDlgButtonChecked(window,ID_RESTORE_POINT)!=BST_CHECKED) ok=FALSE;
                GetWindowTextW(w->checks[A_EXTENSIONS],text,ARRAYSIZE(text));
                if (wcscmp(text,tr(actions[A_EXTENSIONS].vi,actions[A_EXTENSIONS].en))) ok=FALSE;
                GetDlgItemTextW(window,ID_SEARCH,text,ARRAYSIZE(text));
                if (wcscmp(text,L"extensions")) ok=FALSE;
                GetWindowTextW(w->detail,text,ARRAYSIZE(text));
                if (!wcsstr(text,tr(details_vi[A_EXTENSIONS],actions[A_EXTENSIONS].detail))) ok=FALSE;
            }
        }
        if (!bloat) {
            SetDlgItemTextW(window,ID_SEARCH,L"no_such_setting_938");
            for (unsigned a=0;a<ACTION_COUNT;++a) if (w->visible[a]) ok=FALSE;
            SetDlgItemTextW(window,ID_SEARCH,L"");
            SetWindowPos(window,NULL,0,0,900,740,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
            setup_scroll(w,100000);
            if (w->scroll<=0) ok=FALSE;
            SendMessageW(window,WM_COMMAND,ID_NONE,0);
            if (SendMessageW(w->checks[A_EXTENSIONS],BM_GETCHECK,0,0)!=BST_UNCHECKED) ok=FALSE;
            if (IsDlgButtonChecked(window,ID_RESTORE_POINT)!=BST_CHECKED) ok=FALSE;
        }
        DestroyWindow(window);
    }
    g_vi=original;
    return ok;
}
BOOL optimizer_self_test(void) {
    if (!packages_self_test()) return FALSE;
    for (unsigned i=0;i<REG_COUNT;++i) {
        const RegOp *op=&reg_ops[i];
        if (op->action>=ACTION_COUNT || !op->key || !op->name || !*op->name || op->edition>2) return FALSE;
        for (unsigned j=i+1;j<REG_COUNT;++j)
            if (op->root==reg_ops[j].root && !wcscmp(op->key,reg_ops[j].key) && !wcscmp(op->name,reg_ops[j].name)) return FALSE;
    }
    Snapshot *s=calloc(1,sizeof(*s)); if (!s) return FALSE;
    s->magic=0x57555453; s->version=SNAP_VERSION; s->count=1;
    BOOL ok=snapshot_valid(s);
    s->values[0].id=SNAP_MAX; ok=ok && !snapshot_valid(s);
    s->values[0].id=0; s->values[0].size=SNAP_DATA+1; ok=ok && !snapshot_valid(s);
    s->values[0].size=0; s->count=2; ok=ok && !snapshot_valid(s);
    s->count=1; s->values[0].id=OP_STORE_ACL; s->values[0].exists=1;
    s->values[0].size=sizeof(StoreSecurity);
    StoreSecurity *store=(StoreSecurity *)s->values[0].data;
    store->sd_size=0xffffffffu; ok=ok && !snapshot_valid(s);
    store->sd_size=5; memcpy(store->descriptor,L"evil",5*sizeof(wchar_t)); ok=ok && !snapshot_valid(s);
    store->sd_size=5; memset(store->descriptor,0xff,5*sizeof(wchar_t)); ok=ok && !snapshot_valid(s);
    ZeroMemory(&s->values[0],sizeof(s->values[0]));
    s->values[0].id=OP_SERVICE; s->values[0].exists=1; s->values[0].size=sizeof(ServiceState);
    ((ServiceState *)s->values[0].data)->start=SERVICE_BOOT_START; ok=ok && !snapshot_valid(s);
    s->values[0].id=0; s->values[0].type=REG_DWORD; s->values[0].size=1; ok=ok && !snapshot_valid(s);
    free(s); return ok;
}
