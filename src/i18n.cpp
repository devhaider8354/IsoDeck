// IsoDeck — Arabic / English strings
#include "i18n.h"

namespace isodeck {

static Lang g_lang = Lang::AR;

struct Pair { const wchar_t* ar; const wchar_t* en; };

static const Pair kStrings[S_COUNT] = {
    /* S_APP_NAME */            { L"IsoDeck", L"IsoDeck" },
    /* S_APP_TAG */             { L"فلاش إقلاع متعدد الأنظمة", L"Multiboot USB creator" },
    /* S_NAV_DEVICES */         { L"الأجهزة", L"Devices" },
    /* S_NAV_ISOS */            { L"ملفات ISO", L"ISO files" },
    /* S_NAV_THEME */           { L"الثيم والقائمة", L"Theme & menu" },
    /* S_NAV_CATALOG */         { L"المكتبة", L"Catalog" },
    /* S_NAV_ABOUT */           { L"الإعدادات", L"Settings" },
    /* S_DEVICES_TITLE */       { L"اختر الفلاش", L"Choose a USB drive" },
    /* S_DEVICES_SUB */         { L"يعرض البرنامج أقراص USB فقط لحمايتك. قرص النظام لا يمكن اختياره أبداً.", L"Only USB drives are shown for safety. The system disk can never be selected." },
    /* S_REFRESH */             { L"تحديث", L"Refresh" },
    /* S_SHOW_ALL_DISKS */      { L"إظهار كل الأقراص (خطر)", L"Show all disks (danger)" },
    /* S_NO_DEVICES */          { L"لا يوجد فلاش موصول", L"No USB drive connected" },
    /* S_NO_DEVICES_HINT */     { L"وصّل الفلاش ثم اضغط تحديث", L"Plug in a USB drive and press Refresh" },
    /* S_VENTOY_INSTALLED */    { L"جاهز للإقلاع", L"Bootable (installed)" },
    /* S_VENTOY_NOT_INSTALLED */{ L"غير مهيأ", L"Not prepared" },
    /* S_SYSTEM_DISK */         { L"قرص النظام", L"System disk" },
    /* S_OPTIONS */             { L"خيارات التهيئة", L"Preparation options" },
    /* S_PART_STYLE */          { L"نمط الأقسام", L"Partition style" },
    /* S_MBR */                 { L"MBR (متوافق مع الأجهزة القديمة)", L"MBR (older PCs)" },
    /* S_GPT */                 { L"GPT (UEFI حديث)", L"GPT (modern UEFI)" },
    /* S_FILESYSTEM */          { L"نظام الملفات", L"File system" },
    /* S_SECURE_BOOT */         { L"دعم Secure Boot", L"Secure Boot support" },
    /* S_RESERVE */             { L"مساحة محجوزة بنهاية الفلاش (MB)", L"Reserved space at end (MB)" },
    /* S_RESERVE_HINT */        { L"0 = بدون حجز", L"0 = none" },
    /* S_INSTALL */             { L"تهيئة الفلاش الآن", L"Prepare drive now" },
    /* S_UPDATE */              { L"تحديث محرك الإقلاع (بدون مسح)", L"Update boot engine (keeps files)" },
    /* S_INSTALL_WARN_TITLE */  { L"تحذير: سيتم مسح كل البيانات", L"Warning: all data will be erased" },
    /* S_INSTALL_WARN */        { L"سيتم مسح كل محتويات الفلاش نهائياً:\n\n%s\n\nهل أنت متأكد؟", L"Everything on this drive will be permanently erased:\n\n%s\n\nAre you sure?" },
    /* S_YES_ERASE */           { L"نعم، امسح وهيّئ", L"Yes, erase and prepare" },
    /* S_CANCEL */              { L"إلغاء", L"Cancel" },
    /* S_WORKING */             { L"جاري العمل…", L"Working…" },
    /* S_PREPARING */           { L"جاري تجهيز الملفات المضمّنة…", L"Extracting embedded files…" },
    /* S_DONE_OK */             { L"تمت التهيئة بنجاح! انسخ ملفات ISO للفلاش وجرّب الإقلاع.", L"Done! Copy ISO files to the drive and boot from it." },
    /* S_DONE_FAIL */           { L"فشلت العملية. افتح السجل لمعرفة السبب.", L"Operation failed. Open the log for details." },
    /* S_OPEN_LOG */            { L"فتح السجل", L"Open log" },
    /* S_NEED_PKG */            { L"الحزمة الأساسية غير موجودة", L"Boot engine package missing" },
    /* S_NEED_PKG_HINT */       { L"هذه النسخة بُنيت بدون حزمة المحرك. راجع README لإعادة البناء.", L"This build was made without the engine package. See README to rebuild." },
    /* S_SELECT_DEVICE_FIRST */ { L"اختر فلاش من صفحة الأجهزة أولاً", L"Select a drive on the Devices page first" },
    /* S_NOT_VENTOY_DISK */     { L"هذا الفلاش غير مهيأ بعد — هيّئه من صفحة الأجهزة", L"This drive is not prepared yet — prepare it on the Devices page" },
    /* S_ISOS_TITLE */          { L"ملفات الإقلاع على الفلاش", L"Boot images on the drive" },
    /* S_ISOS_SUB */            { L"أضف ملفات ISO / WIM / IMG / VHD(x) — كل ملف يظهر تلقائياً بقائمة الإقلاع.", L"Add ISO / WIM / IMG / VHD(x) files — each appears automatically in the boot menu." },
    /* S_ADD_ISO */             { L"إضافة ملفات", L"Add files" },
    /* S_DELETE */              { L"حذف", L"Delete" },
    /* S_VERIFY_SHA */          { L"SHA-256", L"SHA-256" },
    /* S_FREE_SPACE */          { L"المساحة الحرة", L"Free space" },
    /* S_NO_ISOS */             { L"لا توجد ملفات إقلاع بعد", L"No boot images yet" },
    /* S_DROP_HINT */           { L"اسحب ملفات ISO وأفلتها هنا، أو اضغط إضافة ملفات", L"Drag & drop ISO files here, or press Add files" },
    /* S_COPYING */             { L"جاري النسخ", L"Copying" },
    /* S_DELETE_CONFIRM */      { L"حذف الملف نهائياً؟\n%s", L"Permanently delete this file?\n%s" },
    /* S_SHA_RESULT */          { L"SHA-256 للملف %s:\n\n%s\n\n(تم نسخه للحافظة)", L"SHA-256 of %s:\n\n%s\n\n(copied to clipboard)" },
    /* S_OPEN_FOLDER */         { L"فتح الفلاش", L"Open drive" },
    /* S_SHOW_IN_MENU */        { L"الاسم بالقائمة", L"Menu name" },
    /* S_THEME_TITLE */         { L"ثيم قائمة الإقلاع", L"Boot menu theme" },
    /* S_THEME_SUB */           { L"خصّص الشكل الذي يراه الزبون عند الإقلاع من الفلاش.", L"Customize what the customer sees when booting from the drive." },
    /* S_BRAND_TEXT */          { L"نص العلامة (أحرف لاتينية)", L"Brand text (Latin letters)" },
    /* S_ACCENT */              { L"اللون الأساسي", L"Accent color" },
    /* S_BACKGROUND */          { L"خلفية القائمة", L"Menu background" },
    /* S_CHOOSE_IMAGE */        { L"اختيار صورة…", L"Choose image…" },
    /* S_USE_DEFAULT_BG */      { L"الخلفية الافتراضية", L"Default background" },
    /* S_MENU_TIMEOUT */        { L"مهلة القائمة (ثانية)", L"Menu timeout (seconds)" },
    /* S_WIN11_BYPASS */        { L"تجاوز متطلبات Windows 11 (TPM/RAM)", L"Bypass Windows 11 requirements (TPM/RAM)" },
    /* S_MENU_ALIASES */        { L"أسماء الملفات بالقائمة", L"Menu names for images" },
    /* S_ALIAS_HINT */          { L"اترك الحقل فارغاً لاستخدام اسم الملف", L"Leave empty to use the file name" },
    /* S_UNATTEND */            { L"تثبيت تلقائي", L"Auto-install" },
    /* S_UNATTEND_PICK */       { L"اختيار unattend.xml", L"Pick unattend.xml" },
    /* S_UNATTEND_CLEAR */      { L"إزالة", L"Remove" },
    /* S_APPLY_THEME */         { L"تطبيق على الفلاش", L"Apply to drive" },
    /* S_THEME_APPLIED */       { L"تم تطبيق الثيم وحفظ ventoy.json على الفلاش.", L"Theme applied and ventoy.json saved to the drive." },
    /* S_THEME_FAILED */        { L"تعذّر الكتابة على الفلاش.", L"Could not write to the drive." },
    /* S_ALIAS_LATIN_NOTE */    { L"ملاحظة: قائمة الإقلاع لا تدعم عرض العربية بشكل صحيح، استخدم أحرفاً لاتينية.", L"Note: the boot menu cannot render Arabic properly; use Latin letters." },
    /* S_PREVIEW */             { L"معاينة", L"Preview" },
    /* S_CATALOG_TITLE */       { L"مكتبة الأنظمة والأدوات", L"Systems & tools catalog" },
    /* S_CATALOG_SUB */         { L"نزّل ملفات ISO مباشرة إلى الفلاش المحدد.", L"Download ISO files straight to the selected drive." },
    /* S_DOWNLOAD */            { L"تنزيل للفلاش", L"Download to drive" },
    /* S_OPEN_PAGE */           { L"فتح صفحة التنزيل", L"Open download page" },
    /* S_CUSTOM_URL */          { L"رابط مباشر مخصص", L"Custom direct URL" },
    /* S_DOWNLOADING */         { L"جاري التنزيل", L"Downloading" },
    /* S_DOWNLOAD_DONE */       { L"اكتمل التنزيل", L"Download complete" },
    /* S_DOWNLOAD_FAIL */       { L"فشل التنزيل", L"Download failed" },
    /* S_EDIT_CATALOG */        { L"تعديل المكتبة", L"Edit catalog" },
    /* S_CATALOG_NOTE */        { L"الروابط قابلة للتعديل من ملف catalog.txt — تحقق من الإصدارات قبل التوزيع.", L"Links are editable in catalog.txt — verify versions before distributing." },
    /* S_ABOUT_TITLE */         { L"الإعدادات وحول البرنامج", L"Settings & about" },
    /* S_LANGUAGE */            { L"لغة الواجهة", L"Interface language" },
    /* S_ARABIC */              { L"العربية", L"Arabic" },
    /* S_ENGLISH */             { L"English", L"English" },
    /* S_VERSION */             { L"الإصدار", L"Version" },
    /* S_CREDITS */             { L"تطوير حيدر — FLOW POINT", L"Developed by Haider — FLOW POINT" },
    /* S_LICENSE_NOTE */        { L"يتضمن مكونات مفتوحة المصدر (GPLv3) — التفاصيل في ملف LICENSE.", L"Includes open-source components (GPLv3) — see the LICENSE file." },
    /* S_OPEN_DATA_DIR */       { L"فتح مجلد البيانات", L"Open data folder" },
    /* S_VENTOY_VERSION */      { L"إصدار المحرك المضمّن", L"Embedded engine version" },
    /* S_ADMIN_NEEDED */        { L"يحتاج البرنامج صلاحيات المسؤول للكتابة على الأقراص.", L"Administrator rights are required to write to disks." },
    /* S_CLOSE */               { L"إغلاق", L"Close" },
    /* S_MINIMIZE */            { L"تصغير", L"Minimize" },
    /* S_OK_BTN */                  { L"حسناً", L"OK" },
    /* S_BUSY_EXIT */           { L"هناك عملية جارية. الخروج الآن قد يتلف الفلاش. هل تريد الخروج؟", L"An operation is running. Exiting now may corrupt the drive. Exit anyway?" },
    /* S_ERASE_KEYWORD */       { L"سيتم المسح", L"WILL ERASE" },
};

void SetLang(Lang l) { g_lang = l; }
Lang GetLang() { return g_lang; }
bool IsRTL() { return g_lang == Lang::AR; }
const Str& T(S id) {
    static Str cache[2][S_COUNT];
    int li = (int)g_lang;
    if (cache[li][id].empty()) cache[li][id] = (g_lang == Lang::AR) ? kStrings[id].ar : kStrings[id].en;
    return cache[li][id];
}

} // namespace isodeck
