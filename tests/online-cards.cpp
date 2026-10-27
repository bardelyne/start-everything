#define NOMINMAX
#include <windows.h>
#include <roapi.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cwctype>
#include <functional>
#include <initializer_list>
#include <locale>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <array>
#include <limits>
using namespace std::chrono_literals;
// PRODUCTION_I18N
// PRODUCTION_TOOLS
// PRODUCTION_NUMBERS
// PRODUCTION_COMMANDS
enum class CustomCommandShell { Cmd };
std::mutex g_resultsMutex, g_queryMutex;
std::condition_variable g_queryWake;
std::atomic<bool> g_searchQuit{false};
std::atomic<DWORD> g_totalMatches{0};
uint64_t g_queryRevision = 0;
std::optional<std::thread> g_onlineThread;
// PRODUCTION_MODEL
std::atomic<int> renders{0};
void RequestRender() { ++renders; }
void Wh_Log(const wchar_t*, ...) {}
// PRODUCTION_ONLINE
std::vector<Row> g_currentAppRows, g_currentFileRows;
uint64_t g_shownAppsRevision = 0, g_shownFilesRevision = 0, g_shownAppsContentRevision = 0;
DWORD g_shownFilesTotal = 0;
bool g_filesColumnActive = false;
int g_selectedApp = 0;
bool FilesPanelVisible() { return false; }
// PRODUCTION_SNAPSHOT
// PRODUCTION_PENDING

int checks = 0;
void Check(bool value, const char* label) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", label); std::exit(1); }
    ++checks;
}
template<class F> void Reject(F function, const char* label) {
    bool rejected = false; try { function(); } catch (...) { rejected = true; } Check(rejected, label);
}
online::Request Parse(const wchar_t* query, const wchar_t* city = L"auto", const wchar_t* units = L"celsius") {
    return online::Parse(query, city, units, L"USD", true);
}
online::Json Json(const wchar_t* text) { return online::Json::Parse(text); }
const wchar_t* forecast = LR"({"utc_offset_seconds":10800,"current":{"temperature_2m":27.5,"relative_humidity_2m":35,"weather_code":0,"is_day":1,"wind_speed_10m":8.4,"time":"2026-10-26T12:00"},"daily":{"time":["2026-10-24","2026-10-25","2026-10-26","2026-10-27","2026-10-28","2026-10-29","2026-10-30","2026-10-31"],"temperature_2m_min":[10,11,19.2,17,18,16,17,18],"temperature_2m_max":[20,21,31.4,29,30,28,29,30],"weather_code":[3,3,0,1,2,3,61,2]}})";
online::Json WeatherFixture() {
    using Value=winrt::Windows::Data::Json::JsonValue;
    auto json=Json(forecast);
    const auto today=online::CityDate(10800);
    json.GetNamedObject(L"current").SetNamedValue(L"time",Value::CreateStringValue(today+L"T12:00"));
    SYSTEMTIME time{}; online::Date(today,&time);
    FILETIME file{}; SystemTimeToFileTime(&time,&file);
    ULARGE_INTEGER ticks{};ticks.LowPart=file.dwLowDateTime;ticks.HighPart=file.dwHighDateTime;
    auto dates=json.GetNamedObject(L"daily").GetNamedArray(L"time");
    for(int i=0;i<8;++i){
        ULARGE_INTEGER date{};date.QuadPart=static_cast<ULONGLONG>(static_cast<LONGLONG>(ticks.QuadPart)+(i-2)*86400LL*10000000LL);
        FILETIME day{date.LowPart,date.HighPart};SYSTEMTIME value{};FileTimeToSystemTime(&day,&value);
        wchar_t text[16];swprintf_s(text,L"%04u-%02u-%02u",value.wYear,value.wMonth,value.wDay);
        dates.SetAt(i,Value::CreateStringValue(text));
    }
    return json;
}

int main(int argc, char** argv) {
    const HRESULT apartment = RoInitialize(RO_INIT_MULTITHREADED);
    i18n::Load(L"en");
    if (argc>1 && (std::string(argv[1])=="--live-speed" || std::string(argv[1])=="--measure-speed" || std::string(argv[1])=="--measure-speed-small" || std::string(argv[1])=="--measure-speed-timed")) {
        auto request=Parse(L"/speedtest");request.runSpeed=true;request.revision=1;request.speedId=1;
        g_queryRevision=online::g_speedRevision=1;online::g_speedId=1;online::g_speedBusy=true;
        try {
            online::Http http;
            if(std::string(argv[1])=="--measure-speed-timed"){
                SpeedView view;size_t total=0;
                for(bool upload:{false,true}){
                    view.phase=upload?SpeedView::Phase::Upload:SpeedView::Phase::Download;view.progress=upload?55:10;
                    const auto began=std::chrono::steady_clock::now();
                    const auto measured=online::MeasureDirection(request,upload,view,[](const SpeedView&){},0,1.0);
                    const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();
                    Check(measured.mbps>0 && measured.seconds==1 && !measured.limited && elapsed>=1 && elapsed<12,"native timer cuts off active requests and keeps the full measurement window");
                    Check(online::g_activeHttp==nullptr,"native deadline leaves no active requests behind");total+=measured.bytes;
                }
                std::printf("PASS: native timed LibreSpeed download and upload (1 s each; %.2f MB measured)\n",total/1e6);return 0;
            }
            if(std::string(argv[1])=="--measure-speed-small"){
                SpeedView view;double previous=0;
                for(bool upload:{false,true}){
                    view.phase=upload?SpeedView::Phase::Upload:SpeedView::Phase::Download;
                    view.progress=upload?55:10;
                    auto measured=online::MeasureDirection(request,upload,view,[&](const SpeedView& value){
                        Check(value.progress>=previous && value.progress<=100,"parallel live progress remains monotonic");previous=value.progress;
                    },2000000,1.0);
                    Check(measured.mbps>0 && measured.bytes<=2000000,"bounded parallel transfer produces measured throughput");
                }
                std::puts("PASS: bounded parallel LibreSpeed download and upload (4 MB cap)");return 0;
            }
            if(std::string(argv[1])=="--measure-speed"){
                double previous=0;size_t updates=0;
                auto phase=SpeedView::Phase::Ready;double began=0;
                const auto measured=online::MeasureSpeed(http,request,[&](const SpeedView& value){
                    if(value.phase!=phase){
                        if(value.phase==SpeedView::Phase::Latency && !value.server.empty())std::printf("LibreSpeed backend: %ls\n",value.server.c_str());
                        const auto now=online::SpeedClock();
                        if(phase==SpeedView::Phase::Download || phase==SpeedView::Phase::Upload)
                            Check(now-began>=5 && now-began<30,"live adaptive measurement includes warmup and stays bounded");
                        phase=value.phase;began=now;
                    }
                    Check(std::isfinite(value.progress)&&value.progress>=previous&&value.progress<=100,"real speed progress is monotonic");
                    previous=value.progress;++updates;
                });
                Check(measured.phase==SpeedView::Phase::Done&&measured.latency>0&&measured.download>0&&measured.upload>0,"real speed measurements complete");
                std::printf("Learned upload body limit: %.2f MiB\n",online::g_speedUploadLimits[online::g_speedServer]/1048576.0);
                std::printf("PASS: LibreSpeed adaptive download + upload; %.1f / %.1f Mbps; %zu updates\n",measured.download,measured.upload,updates);return 0;
            }
            const auto down=online::Transfer(http,request,false,65536,[](size_t){});
            const auto up=online::Transfer(http,request,true,65536,[](size_t){});
            Check(down.bytes==65536 && up.bytes==65536,"LibreSpeed accepts download and synthetic upload payloads");
            std::puts("PASS: LibreSpeed 64 KiB download + 64 KiB synthetic upload probe");
            return 0;
        } catch(const std::exception& error) {
            std::fprintf(stderr,"FAIL: LibreSpeed transfer probe: %s (Windows error %lu)\n",error.what(),GetLastError());return 1;
        } catch(...) {std::fputs("FAIL: LibreSpeed transfer probe\n",stderr);return 1;}
    }
    const commands::Names localized = commands::Resolve({L"/m\u00E9t\u00E9o",L"devises",L"coins",L"adresse"});
    Check(commands::Parse(L"/M\u00C9T\u00C9O London",localized).kind == commands::Kind::Weather, "Unicode command names are case insensitive");
    Check(commands::Parse(L"/adresse",localized).kind == commands::Kind::Ip, "IP command can be renamed");
    Check(commands::Parse(L"/ip",localized).kind == commands::Kind::None, "renamed IP command uses its configured name");
    Check(commands::Parse(L"/money",localized).kind == commands::Kind::None, "renamed currency command replaces default name");
    Check(commands::Parse(L"/fx",localized).kind == commands::Kind::None, "default aliases do not bypass a renamed command");
    auto chinese = commands::Resolve({L"\u5929\u6C14",L"\u8D27\u5E01",L"\u52A0\u5BC6",L"\u5730\u5740"});
    Check(commands::Parse(L"/\u5929\u6C14",chinese).kind == commands::Kind::Weather, "non-Latin command names work");
    auto swapped = commands::Resolve({L"money",L"weather",L"crypto",L"ip"});
    Check(commands::Parse(L"/money",swapped).kind == commands::Kind::Weather && commands::Parse(L"/weather",swapped).kind == commands::Kind::Currency, "valid command-name swaps work");
    auto collisions = commands::Resolve({L"same",L"SAME",L"same",L"same"});
    Check(commands::Parse(L"/same",collisions).kind == commands::Kind::Conflict && commands::Parse(L"/weather",collisions).kind == commands::Kind::None, "colliding names are reported without executing a command");
    Check(online::ConflictRow().isError && online::ConflictRow().infoOnly, "command-name conflict produces a non-actionable error card");
    for (const auto* name : {L"",L"two words",L"//weather",L"bad\u202Ename",L"wrong>name"})
        Check(commands::Normalize(name).empty(), "invalid command name rejected");
    auto translatedWeather = online::Parse(L"/m\u00E9t\u00E9o",L"auto",L"celsius",L"USD",true,localized);
    Check(translatedWeather.valid && online::Hint(translatedWeather).find(L"/m\u00E9t\u00E9o London") != std::wstring::npos, "help uses the configured command name");
    Check(!online::Date(L"2026-02-30") && online::Date(L"2024-02-29") && !online::Date(L"2026-02-29"), "forecast dates validate leap days");
    if (argc > 1 && std::string(argv[1]) == "--live") {
        online::Http http;
        for (const auto* query : {L"/money USD IQD", L"/crypto BTC USD", L"/weather Baghdad"}) {
            const auto request = Parse(query);
            try {
                const auto data = online::Fetch(request, [&](const auto& host, const auto& path) { return http.Get(host,path); });
                Check(!online::Rows(request,data).empty(), "live API returned a usable card");
                std::printf("PASS live provider kind %d\n", static_cast<int>(request.kind));
            } catch (...) { std::fprintf(stderr,"FAIL live provider kind %d\n",static_cast<int>(request.kind)); return 1; }
        }
        if (SUCCEEDED(apartment)) RoUninitialize();
        return 0;
    }
    for (const auto* query : {L"brave",L"weather London",L"BTC",L"100 USD EUR",L"?weather",L"/ip",L"/weathers",L"/moneybox"})
        Check(Parse(query).kind == online::Kind::None, "ordinary searches never become online queries");
    const auto currency = Parse(L"/money 100 USD > EUR");
    auto moneyRoot = Parse(L"/money"); moneyRoot.revision = 8;
    auto savedMoney = online::Expand(moneyRoot,{}, {}, {L"EUR",L"100 USD > EUR",L"eur",L"200 USD > EUR"}, {});
    Check(savedMoney.size() == 3, "bare currency command expands and deduplicates saved cards");
    Check(savedMoney[0].base == L"EUR" && savedMoney[0].quote == L"USD" && savedMoney[1].amount == 100, "saved codes and full conversions parse");
    Check(std::all_of(savedMoney.begin(),savedMoney.end(),[&](const auto& row){return row.query == moneyRoot.query && row.revision == 8;}), "saved cards keep the parent query identity");
    Check(online::Expand(currency,{}, {}, {L"GBP"}, {}).size() == 1, "explicit conversion bypasses saved cards");
    auto savedCrypto = online::Expand(Parse(L"/crypto"),{}, {}, {}, {L"BTC",L"0.5 ETH > EUR"});
    Check(savedCrypto.size() == 2 && savedCrypto[1].amount == 0.5, "bare crypto command expands saved amounts");
    auto savedWeather = online::Expand(Parse(L"/weather"),{}, {L"London",L"london",L"Tokyo",L"auto"}, {}, {});
    Check(savedWeather.size() == 3 && savedWeather.front().city.empty(), "weather includes primary location and unique saved cities");
    Check(online::Expand(Parse(L"/weather Paris"),{}, {L"London"}, {}, {}).size() == 1, "explicit weather location bypasses saved cities");
    Check(online::BeginBatch(savedMoney).rows.size() == 3 && online::BeginBatch(savedMoney).fetch, "saved cards immediately show independent loading rows");
    Check(!online::BeginBatch(savedMoney).rows.front().sectionEmoji.empty(), "currency header uses emoji instead of unsupported private glyph");
    Check(!online::BeginBatch(savedCrypto).rows.front().sectionEmoji.empty() && online::Glyph(online::Kind::Crypto) != std::wstring(L"\uEB0D"), "crypto header avoids missing glyph");
    auto speed = Parse(L"/speedtest");
    Check(speed.kind == online::Kind::Speed && speed.valid && !speed.runSpeed, "typing speed command does not start a transfer");
    Check(!online::Begin(speed).fetch && online::Begin(speed).rows.front().speedStart, "speed command opens a ready card");
    Check(Parse(L"/speed").kind == online::Kind::Speed && !Parse(L"/speedtest extra").valid, "speed alias and argument validation");
    Check(std::abs(online::Mbps({1000000,1,0})-8)<1e-9, "throughput uses measured bits per second");
    Reject([]{online::Mbps({0,1,0});},"empty transfer is not a speed result");
    Reject([]{online::Mbps({1000,0,0});},"zero duration is not a speed result");
    std::vector<std::wstring> many;
    for (int i=0;i<30;++i) many.push_back(std::to_wstring(i+1)+L" USD EUR");
    Check(online::Expand(moneyRoot,{}, {}, many, {}).size() == 12, "saved money card count is bounded");
    Check(currency.valid && currency.amount == 100 && currency.base == L"USD" && currency.quote == L"EUR", "currency command parses");
    Check(Parse(L"/fx eur in usd").valid && Parse(L"/fx eur in usd").quote == L"USD", "currency alias and in separator");
    Check(Parse(L"/currency 1 000 USD \u2192 IQD").amount == 1000, "grouped amounts and arrow separator");
    Check(std::abs(Parse(L"/crypto 0.5 ETH > EUR").amount - 0.5) < 1e-9, "fractional crypto conversion");
    Check(Parse(L"/price bitcoin").base == L"BTC", "crypto name alias");
    Check(Parse(L"/crypto SOL").quote == L"USD", "default quote currency");
    Check(Parse(L"/crypto BTC/USDT").valid && Parse(L"/crypto BTC/USDT").quote == L"USDT", "crypto quote symbols and slash separator");
    for (const auto* query : {L"/money",L"/crypto",L"/money -1 USD EUR",L"/money 1e309 USD EUR",L"/money 1e16 USD EUR",L"/crypto BTC EUR extra",L"/crypto ../ BTC",L"/money USD BADCODE"})
        Check(!Parse(query).valid, "malformed money command is local help, not a request");
    Check(Parse(L"/weather").valid && Parse(L"/weather").city.empty(), "weather auto location");
    Check(Parse(L"/weather Paris, France").city == L"Paris, France", "weather explicit city override");
    Check(Parse(L"/weather",L"London").city == L"London", "default city avoids auto lookup");
    Check(Parse(L"/weather London",L"auto",L"fahrenheit").fahrenheit, "Fahrenheit setting");
    Check(!Parse((L"/weather " + std::wstring(121,L'x')).c_str()).valid, "overlong city rejected");
    Check(online::Escape(L"A&B #/\u00E9") == L"A%26B%20%23%2F%C3%A9", "city encoded as UTF-8 parameter");
    Check(online::AllowedHost(L"api.coinbase.com") && !online::AllowedHost(L"api.coinbase.com.evil.test"), "exact provider allowlist");
    Check(online::Decimal(L"1234.567") == 1234.567, "API decimals are independent of user locale");
    for (const auto* value : {L"nan",L"inf",L"1.2evil",L"1,234",L"1e999",L""})
        Reject([&]{online::Decimal(value);}, "bad API decimal rejected");

    int requests = 0;
    const auto rate = online::Fetch(currency, [&](const auto& host, const auto& path) {
        ++requests;
        Check(host == L"api.frankfurter.dev" && path == L"/v2/rate/USD/EUR", "amount is never sent to currency provider");
        return Json(LR"({"base":"USD","quote":"EUR","date":"2026-10-25","rate":0.9})");
    });
    Check(rate.value == 0.9 && online::Rows(currency,rate)[0].copyText == L"90", "currency conversion uses fetched rate");
    Check(online::Rows(Parse(L"/money 200 USD EUR"),rate)[0].copyText == L"180" && requests == 1, "amount changes calculate locally");
    Reject([&]{online::Fetch(currency,[](const auto&,const auto&){ return Json(LR"({"base":"GBP","quote":"EUR","date":"2026-10-25","rate":0.9})"); });}, "wrong currency pair rejected");
    Reject([&]{online::Fetch(currency,[](const auto&,const auto&){ return Json(LR"({"base":"USD","quote":"EUR","date":"2026-10-25","rate":-1})"); });}, "negative rate rejected");
    const auto crypto = Parse(L"/crypto 0.5 BTC EUR");
    const auto price = online::Fetch(crypto,[](const auto& host,const auto& path) {
        Check(host == L"api.coinbase.com" && path == L"/v2/prices/BTC-EUR/spot", "crypto endpoint contains pair only");
        return Json(LR"({"data":{"base":"BTC","currency":"EUR","amount":"80000.25"}})");
    });
    Check(online::Rows(crypto,price)[0].copyText == L"40000.125", "crypto conversion uses spot price");
    Reject([&]{online::Fetch(crypto,[](const auto&,const auto&){return Json(LR"({"data":{"base":"ETH","currency":"EUR","amount":"1"}})");});}, "wrong crypto base rejected");

    int autoCalls = 0, geoCalls = 0;
    auto weatherApi = [&](const std::wstring& host,const std::wstring& path) {
        if (host == L"ipwho.is") { ++autoCalls; Check(path.find(L"fields=") != std::wstring::npos, "auto lookup requests only city fields"); return Json(LR"({"success":true,"city":"Example City","country":"Example Country","latitude":33.3,"longitude":44.4})"); }
        if (host == L"geocoding-api.open-meteo.com") { ++geoCalls; return Json(LR"({"results":[{"name":"London","country":"United Kingdom","latitude":51.5,"longitude":-0.1}]})"); }
        Check(host == L"api.open-meteo.com" && path.find(L"latitude=") != std::wstring::npos, "weather uses coordinates");
        return WeatherFixture();
    };
    const auto weather = online::Fetch(Parse(L"/weather"), weatherApi);
    Check(weather.days.size() == 8 && weather.days[2].date == online::CityDate(10800), "weather has past, current and future days in order");
    Check(weather.low == 19.2 && weather.high == 31.4, "today temperature range is not taken from first past day");
    const auto weatherCard = online::Rows(Parse(L"/weather"),weather)[0];
    Check(weatherCard.weatherDays.size() == 8 && !weatherCard.cardSignature.empty(), "one weather card contains the complete daily timeline");
    Check(weatherCard.copyText.find(weather.days.front().date) != std::wstring::npos && weatherCard.copyText.find(weather.days.back().date) != std::wstring::npos, "copy includes history and forecast");
    Check(std::count_if(weatherCard.weatherDays.begin(),weatherCard.weatherDays.end(),[](const auto& d){return d.today;}) == 1, "exactly one day receives today's highlight");
    Check(online::DayLabel(L"2026-10-26",true) == i18n::Text(i18n::Key::Today), "today label is localized");
    auto changedWeather = weather; changedWeather.days.back().high += 1;
    Check(online::Rows(Parse(L"/weather"),changedWeather)[0].cardSignature != weatherCard.cardSignature, "future weather changes invalidate a reused card");
    Reject([&]{ online::Fetch(Parse(L"/weather"),[&](const auto&,const auto&){auto json=WeatherFixture();json.GetNamedObject(L"daily").GetNamedArray(L"weather_code").RemoveAtEnd();return json;}); }, "mismatched daily arrays rejected");
    Check(weather.automatic && weather.location == L"Example City, Example Country", "auto weather card labels approximate city");
    online::Fetch(Parse(L"/weather"),weatherApi);
    Check(autoCalls == 1, "auto location reused for six hours");
    online::Fetch(Parse(L"/weather London"),weatherApi);
    online::Fetch(Parse(L"/weather London"),weatherApi);
    Check(geoCalls == 1 && autoCalls == 1, "manual city uses cached geocoding and no IP lookup");
    Check(online::Rows(Parse(L"/weather",L"auto",L"fahrenheit"),weather)[0].title.starts_with(L"82 \u00B0F"), "weather converted to Fahrenheit locally");
    Reject([&]{online::Fetch(Parse(L"/weather Missing"),[](const auto&,const auto&){return Json(LR"({"results":[]})");});}, "missing city is an error, not arbitrary location");
    Reject([&]{online::Fetch(Parse(L"/weather"),[](const auto&,const auto&){return Json(LR"({"current":{},"daily":{}})");});}, "incomplete weather rejected");

    const auto now = GetTickCount64();
    auto oldWeather=weather;oldWeather.today=L"1900-01-01";
    online::Store(online::Key(Parse(L"/weather")),{oldWeather,now+60000,now});
    Check(online::Begin(Parse(L"/weather")).fetch,"weather refreshes after the city's date rolls over");
    online::Store(online::Key(Parse(L"/weather")),{oldWeather,now+60000,now,true});
    Check(!online::Begin(Parse(L"/weather")).fetch,"date rollover still respects failed-request backoff");
    online::Store(online::Key(currency), {rate,now+60000,now});
    Check(!online::Begin(Parse(L"/money 200 USD EUR")).fetch, "fresh pair cache reused for different amount");
    online::Store(online::Key(currency), {rate,now,now});
    const auto stale = online::Begin(currency);
    Check(stale.fetch && stale.rows[0].subtitle.find(i18n::Text(i18n::Key::Cached)) != std::wstring::npos, "expired cached rate visibly marked");
    online::Store(online::Key(currency), {rate,now+60000,now,true});
    const auto backoff = online::Begin(currency);
    Check(!backoff.fetch && backoff.rows[0].subtitle.find(i18n::Text(i18n::Key::Cached)) != std::wstring::npos, "failed refresh keeps stale label during backoff");
    online::Store(online::Key(crypto), {std::nullopt,now+60000,now});
    Check(!online::Begin(crypto).fetch && online::Begin(crypto).rows[0].infoOnly, "provider errors back off without fake prices");
    auto disabled = currency; disabled.enabled = false;
    Check(!online::Begin(disabled).fetch && online::Begin(disabled).rows[0].infoOnly, "offline mode prevents even cached online cards");
    for (int i = 0; i < 64; ++i) online::Store(L"key"+std::to_wstring(i),{rate,now+60000,now+static_cast<ULONGLONG>(i)});
    Check(online::g_cache.size() == 32, "online result cache bounded");

    g_queryRevision = 1;
    auto loading = online::Notice(currency,L"Currency",L"Loading",true);
    Check(PublishAppResults(1,L"/money 100 USD EUR",{loading}), "initial online placeholder publishes");
    g_currentAppRows = g_appRows; g_shownAppsRevision = 1; g_shownAppsContentRevision = g_appsContentRevision;
    Check(ResultsPending(), "Enter can wait for an online card");
    Check(PublishAppResults(1,L"/money 100 USD EUR",online::Rows(currency,rate)), "online completion publishes with same query revision");
    const auto snapshot = ReadSearchRenderSnapshot(true,false);
    Check(snapshot && snapshot->appsChanged && !snapshot->filesChanged, "same-query online completion redraws apps only");
    g_currentAppRows = g_appRows; g_shownAppsContentRevision = g_appsContentRevision;
    Check(!ResultsPending(), "Enter proceeds after data arrives");

    // Block an actual online worker at its provider boundary, then change the
    // query. The finished old response must not overwrite the new app result.
    online::g_cache.clear(); renders = 0;
    std::mutex gate; std::condition_variable wake; bool entered=false, release=false;
    auto request = currency; request.revision = 2;
    { std::lock_guard lock(g_queryMutex); g_queryRevision = 2; online::g_request = online::Batch{request}; }
    std::thread worker([&] {
        const auto hr = RoInitialize(RO_INIT_MULTITHREADED);
        online::Run([&](const auto&,const auto&) {
            std::unique_lock lock(gate); entered = true; wake.notify_all(); wake.wait(lock,[&]{return release;});
            return Json(LR"({"base":"USD","quote":"EUR","date":"2026-10-25","rate":0.9})");
        });
        if(SUCCEEDED(hr))RoUninitialize();
    });
    { std::unique_lock lock(gate); Check(wake.wait_for(lock,3s,[&]{return entered;}), "online worker reaches delayed provider"); }
    { std::lock_guard lock(g_queryMutex); g_queryRevision = 3; }
    Row app; app.title=L"Normal app";
    Check(PublishAppResults(3,L"normal",{app}), "ordinary app result publishes while online request is blocked");
    { std::lock_guard lock(gate); release=true; } wake.notify_all();
    std::this_thread::sleep_for(100ms);
    { std::lock_guard lock(g_queryMutex); g_searchQuit=true; } online::Wake(); worker.join();
    Check(g_appRows.size()==1 && g_appRows[0].title==L"Normal app" && renders.load()==0, "late online response cannot replace newer search");
    Check(!g_onlineThread, "unit tests never start real online HTTP worker");

    // Saved amounts using the same pair share one API call and publish each
    // completed card without waiting for the rest of the list.
    online::g_cache.clear(); renders=0;
    auto listRoot=Parse(L"/money"); listRoot.revision=4;
    const auto list=online::Expand(listRoot,{}, {}, {L"100 USD EUR",L"200 USD EUR"}, {});
    {std::lock_guard lock(g_queryMutex);g_searchQuit=false;g_queryRevision=4;online::g_request=list;}
    std::atomic<int> pairCalls{0};
    std::thread batchWorker([&]{
        const auto hr=RoInitialize(RO_INIT_MULTITHREADED);
        online::Run([&](const auto&,const auto&){++pairCalls;return Json(LR"({"base":"USD","quote":"EUR","date":"2026-10-25","rate":0.9})");});
        if(SUCCEEDED(hr))RoUninitialize();
    });
    for(int i=0;i<150 && renders.load()<2;++i) std::this_thread::sleep_for(20ms);
    {std::lock_guard lock(g_queryMutex);g_searchQuit=true;}online::Wake();batchWorker.join();
    Check(renders.load()>=2 && pairCalls.load()==1, "saved amounts share a rate and publish progressively");
    Check(g_appRows.size()==2 && g_appRows[0].copyText==L"90" && g_appRows[1].copyText==L"180", "saved list results retain order and amounts");
    online::g_cache.clear(); renders=0;
    auto mixedRoot=Parse(L"/money"); mixedRoot.revision=5;
    const auto mixed=online::Expand(mixedRoot,{}, {}, {L"EUR",L"XYZ",L"GBP"}, {});
    {std::lock_guard lock(g_queryMutex);g_searchQuit=false;g_queryRevision=5;online::g_request=mixed;}
    std::thread mixedWorker([&]{
        const auto hr=RoInitialize(RO_INIT_MULTITHREADED);
        online::Run([&](const std::wstring&,const std::wstring& path){
            if(path.find(L"XYZ")!=std::wstring::npos)throw std::runtime_error("unsupported currency");
            return path.find(L"GBP")!=std::wstring::npos ?
                Json(LR"({"base":"GBP","quote":"USD","date":"2026-10-25","rate":1.3})") :
                Json(LR"({"base":"EUR","quote":"USD","date":"2026-10-25","rate":1.1})");
        });
        if(SUCCEEDED(hr))RoUninitialize();
    });
    for(int i=0;i<150 && renders.load()<3;++i) std::this_thread::sleep_for(20ms);
    {std::lock_guard lock(g_queryMutex);g_searchQuit=true;}online::Wake();mixedWorker.join();
    Check(renders.load()==3 && g_appRows.size()==3, "each saved card completes independently");
    Check(!g_appRows.front().copyText.empty() && g_appRows[1].infoOnly && !g_appRows.back().copyText.empty(), "one failed saved card does not block the others");
    // Test the real speed-state machine with a fake measurement engine. This
    // never contacts LibreSpeed or sends a payload.
    renders=0; online::g_lastSpeed.reset();
    auto speedRequest=speed;speedRequest.revision=6;speedRequest.speedId=1;speedRequest.runSpeed=true;
    {std::lock_guard lock(g_queryMutex);g_searchQuit=false;g_queryRevision=6;online::g_speedRevision=6;online::g_speedId=1;online::g_speedBusy=true;online::g_request=online::Batch{speedRequest};}
    std::thread speedWorker([&]{
        const auto hr=RoInitialize(RO_INIT_MULTITHREADED);
        online::Run([](const auto&,const auto&)->online::Json{throw std::runtime_error("JSON fetch not expected");},
          [](const auto&,const online::SpeedProgress& progress){
            SpeedView value;value.phase=SpeedView::Phase::Latency;value.progress=1;progress(value);
            value.latency=12.3;value.phase=SpeedView::Phase::Download;
            for(int i=0;i<200;++i){value.progress=10+i*0.2;progress(value);}
            value.download=80.1;value.phase=SpeedView::Phase::Upload;value.progress=70;progress(value);
            value.upload=20.2;value.phase=SpeedView::Phase::Done;value.progress=100;progress(value);return value;
          });
        if(SUCCEEDED(hr))RoUninitialize();
    });
    for(int i=0;i<150;++i){{std::lock_guard lock(g_queryMutex);if(!online::g_speedBusy)break;}std::this_thread::sleep_for(20ms);}
    {std::lock_guard lock(g_queryMutex);g_searchQuit=true;}online::Wake();speedWorker.join();
    Check(renders.load()>=4 && renders.load()<12,"speed progress is throttled instead of rebuilding per network chunk");
    Check(g_appRows.size()==1 && g_appRows[0].speed && g_appRows[0].speed->phase==SpeedView::Phase::Done,"speed completion produces a measured card");
    Check(g_appRows[0].copyText.find(L"80.1")!=std::wstring::npos && online::g_lastSpeed.has_value(),"completed measurements can be copied and reopened");
    auto cancelRequest=speed;cancelRequest.revision=7;cancelRequest.speedId=2;cancelRequest.runSpeed=true;
    {std::lock_guard lock(g_queryMutex);g_searchQuit=false;g_queryRevision=7;online::g_speedRevision=7;online::g_speedId=2;online::g_speedBusy=true;online::g_request=online::Batch{cancelRequest};}
    std::atomic<bool> speedEntered{false};
    std::thread cancelWorker([&]{
        const auto hr=RoInitialize(RO_INIT_MULTITHREADED);
        online::Run([](const auto&,const auto&)->online::Json{throw std::runtime_error("not expected");},
          [&](const online::Request& request,const online::SpeedProgress& progress){
            SpeedView value;value.phase=SpeedView::Phase::Download;value.progress=30;progress(value);speedEntered=true;
            while(online::SpeedCurrent(request))std::this_thread::sleep_for(5ms);
            value.phase=SpeedView::Phase::Done;return value;
          });
        if(SUCCEEDED(hr))RoUninitialize();
    });
    for(int i=0;i<150 && !speedEntered.load();++i)std::this_thread::sleep_for(20ms);
    Check(speedEntered.load(),"cancel test reaches active measurement");
    online::StopSpeed(cancelRequest);
    {std::lock_guard lock(g_queryMutex);g_searchQuit=true;}online::Wake();cancelWorker.join();
    Check(g_appRows[0].speed->phase==SpeedView::Phase::Stopped && !online::g_speedBusy,"stop cancels and keeps a restartable card");
    Check(online::g_lastSpeed->download==80.1,"cancelled measurement does not overwrite last complete result");
    auto parallelRequest=speed;parallelRequest.revision=8;parallelRequest.speedId=3;parallelRequest.runSpeed=true;
    {std::lock_guard lock(g_queryMutex);g_searchQuit=false;g_queryRevision=online::g_speedRevision=8;online::g_speedId=3;online::g_speedBusy=true;}
    std::atomic<int> concurrent{0},peak{0};
    const online::SpeedTransfer fakeTransfer=[&](online::Http&,const online::Request&,bool,size_t length,const online::ByteProgress& progress,const online::SpeedCheck& active){
        if(length==1000)return online::SpeedTrial{length,0.001,1};
        const int count=++concurrent;int maximum=peak.load();while(maximum<count&&!peak.compare_exchange_weak(maximum,count)){}
        for(int i=1;i<=4;++i){std::this_thread::sleep_for(2ms);if(!active()){--concurrent;throw std::runtime_error("cancelled");}progress(length*i/4);}
        --concurrent;return online::SpeedTrial{length,0.008,1};
    };
    SpeedView parallelView;parallelView.phase=SpeedView::Phase::Download;parallelView.progress=10;
    auto parallel=online::MeasureDirection(parallelRequest,false,parallelView,[](const auto&){},1000000,0.2,fakeTransfer);
    Check(peak.load()>1 && concurrent.load()==0,"bandwidth uses simultaneous connections and joins every worker");
    Check(parallel.bytes==996000 && parallel.limited && parallel.mbps>0,"parallel download obeys shared byte budget including warmups");
    parallelView.phase=SpeedView::Phase::Upload;parallelView.progress=55;
    auto upload=online::MeasureDirection(parallelRequest,true,parallelView,[](const auto&){},1000000,0.2,fakeTransfer);
    Check(upload.bytes==996000 && upload.limited && upload.mbps>0,"upload counts acknowledged samples within shared budget");
    Check(online::kSpeedTestSeconds==15.0,"adaptive tests retain a bounded maximum measurement window");
    Check(online::SpeedTimeBonus(0,1)==0 && online::SpeedTimeBonus(10000000,1)==2,"adaptive timing leaves slow transfers time and limits fast-test shortening");
    Check(online::SpeedTimeBonus(4000000,0.2)==0.2 && online::SpeedTimeBonus(4000000,0.1)*2==online::SpeedTimeBonus(4000000,0.2),"adaptive timing is independent of UI update frequency");
    Check(online::SpeedTimeBonus(125000,1)<0.04,"megabit connections are not mistaken for gigabit connections by adaptive timing units");
    for(bool sending:{false,true}){
        parallelView.phase=sending?SpeedView::Phase::Upload:SpeedView::Phase::Download;parallelView.progress=sending?55:10;
        const auto started=std::chrono::steady_clock::now();
        const auto timed=online::MeasureDirection(parallelRequest,sending,parallelView,[](const auto&){},0,0.2,fakeTransfer);
        const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
        Check(elapsed>=0.19 && elapsed<1 && timed.seconds==0.2 && !timed.limited,"timed transfer runs the whole window rather than ending at a byte cap");
        Check(timed.mbps>0 && concurrent.load()==0,"deadline finalizes measured throughput and joins active connections");
    }
    bool cancelled=false;
    try {online::MeasureDirection(parallelRequest,false,parallelView,[&](const auto&){std::lock_guard lock(g_queryMutex);++g_queryRevision;},1000000,2,fakeTransfer);}
    catch(...){cancelled=true;}
    Check(cancelled && concurrent.load()==0,"query cancellation joins all parallel transfers");
    {std::lock_guard lock(g_queryMutex);g_queryRevision=8;}
    {
        online::Http http;
        online::Handle connection(WinHttpConnect(http.session.value,online::Backend(parallelRequest).host,INTERNET_DEFAULT_HTTPS_PORT,0));
        online::ActiveRequest first(WinHttpOpenRequest(connection.value,L"GET",L"/",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
        online::ActiveRequest second(WinHttpOpenRequest(connection.value,L"GET",L"/",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
        Check(first.value && second.value,"cancellation fixture allocates local request handles without sending anything");
        online::CancelHttp();online::CancelHttp();
        Check(first.closed && second.closed,"stop closes every active request exactly once");
    }
    Check(online::g_activeHttp==nullptr,"closed request handles leave no dangling registrations");
    Check(online::RetryAfterSeconds(L"120")==120 && online::RetryAfterSeconds(L"0")==1,"numeric Retry-After is honored without immediate retry loops");
    Check(online::RetryAfterSeconds(L"Sat, 01 Jan 2000 00:00:00 GMT")==1 && online::RetryAfterSeconds(L"invalid")==60,"HTTP-date retry instructions and missing headers are bounded safely");
    online::g_speedRetryUntil=0;online::g_speedNextRequest=0;
    std::array<ULONGLONG,4> paced{};std::vector<std::thread> pacing;
    for(size_t i=0;i<paced.size();++i)pacing.emplace_back([&,i]{online::PaceSpeed(parallelRequest,{});paced[i]=GetTickCount64();});
    for(auto& thread:pacing)thread.join();std::sort(paced.begin(),paced.end());
    Check(paced.back()-paced.front()>=750,"all speed requests share the four-per-second gate");
    online::g_speedNextRequest=0;pacing.clear();
    for(size_t i=0;i<paced.size();++i)pacing.emplace_back([&,i]{online::PaceSpeed(parallelRequest,{},1024*1024);paced[i]=GetTickCount64();});
    for(auto& thread:pacing)thread.join();std::sort(paced.begin(),paced.end());
    Check(paced.back()-paced.front()>=96 && paced.back()-paced.front()<600,"bulk transfers keep bounded pacing without the small-probe bandwidth cap");
    online::DeferSpeed(3);const auto blockedUntil=online::g_speedRetryUntil.load();online::DeferSpeed(1);
    Check(online::g_speedRetryUntil.load()>=blockedUntil && online::SpeedRetrySeconds()>0,"concurrent retry instructions cannot shorten cooldown");
    const auto reservedSlot=online::g_speedNextRequest;
    Reject([&]{online::PaceSpeed(parallelRequest,{});},"cooldown rejects requests before touching the network");
    Check(online::g_speedNextRequest==reservedSlot,"a blocked retry consumes no request slot");
    {std::lock_guard lock(g_queryMutex);online::g_speedBusy=false;}
    online::StartSpeed(parallelRequest);
    Check(!g_onlineThread && !online::g_speedBusy && g_appRows.front().speed->retrySeconds>0,"Run during cooldown shows a useful message without scheduling HTTP");
    online::g_speedRetryUntil=0;{std::lock_guard lock(g_queryMutex);online::g_speedBusy=true;}
    std::atomic<int> selectionCalls{0};bool busy=false;
    try{online::Http http;online::g_speedServer=-1;online::SelectSpeedBackend(http,parallelRequest,[&](auto&,const auto&,bool,size_t,const auto&,const auto&)->online::SpeedTrial{++selectionCalls;throw online::SpeedBusy(3);});}
    catch(const online::SpeedBusy&){busy=true;}
    Check(busy && selectionCalls==1,"server selection stops on a rate limit instead of hopping between nodes");
    const online::SpeedTransfer partial=[](auto&,const auto&,bool,size_t size,const online::ByteProgress& progress,const online::SpeedCheck& active)->online::SpeedTrial{
        if(size==1000)return {size,0.001,1};
        size_t bytes=0;while(active()){bytes+=1000;progress(bytes);std::this_thread::sleep_for(5ms);}throw std::runtime_error("deadline cancellation");
    };
    parallelView=SpeedView{};parallelView.phase=SpeedView::Phase::Download;
    const auto streamed=online::MeasureDirection(parallelRequest,false,parallelView,[](const auto&){},0,0.1,partial);
    Check(streamed.mbps>0 && streamed.bytes>0,"a long validated download counts bytes received before the deadline");
    Reject([&]{online::MeasureDirection(parallelRequest,true,parallelView,[](const auto&){},0,0.1,partial);},"unacknowledged uploads are never reported as completed measurements");
    const online::SpeedTransfer steadyUpload=[](auto&,const auto&,bool,size_t size,const auto& progress,const auto& active)->online::SpeedTrial{
        if(size==1000)return {size,0.001,1};
        const auto begin=online::SpeedClock();
        for(size_t sent=0;sent<size;){
            std::this_thread::sleep_for(2ms);if(!active())throw std::runtime_error("cancelled");
            sent=std::min(size,static_cast<size_t>((online::SpeedClock()-begin)*4*1024*1024));progress(sent);
        }
        return {size,online::SpeedClock()-begin,1};
    };
    const auto drained=online::MeasureDirection(parallelRequest,true,parallelView,[](const auto&){},0,0.45,steadyUpload);
    Check(drained.mbps>70 && drained.mbps<150,"a known-rate upload is not undercounted by discarding the final in-flight bodies");
    Check(drained.bytes<4*2*1024*1024 && drained.seconds==0.45,"response drain confirms upload bytes but excludes traffic after the measurement window");
    const auto beganAdaptive=online::SpeedClock();
    const auto adaptive=online::MeasureDirection(parallelRequest,false,parallelView,[](const auto&){},0,0.3,steadyUpload,true);
    Check(online::SpeedClock()-beganAdaptive>=1.5 && adaptive.seconds>=0.09 && adaptive.seconds<=0.3,"adaptive download excludes its connection ramp-up and shortens without waiting for a full long response");
    Check(adaptive.mbps>70 && adaptive.mbps<220,"grace-period traffic is excluded rather than inflated into the short measurement window");
    const online::SpeedTransfer bufferedUpload=[](auto&,const auto&,bool,size_t size,const auto& progress,const auto& active)->online::SpeedTrial{
        if(size==1000)return {size,0.001,1};
        const auto begin=online::SpeedClock();progress(size);
        while(online::SpeedClock()-begin<static_cast<double>(size)/(4*1024*1024)){
            std::this_thread::sleep_for(2ms);if(!active())throw std::runtime_error("cancelled");
        }
        return {size,online::SpeedClock()-begin,1};
    };
    const auto buffered=online::MeasureDirection(parallelRequest,true,parallelView,[](const auto&){},0,0.45,bufferedUpload);
    Check(buffered.mbps>70 && buffered.mbps<150,"buffered writes and late acknowledgments do not inflate a known-rate upload");
    std::atomic<int> rejectedBodies{0};
    const auto bodyLimited=[&](online::Http& http,const online::Request& request,bool upload,size_t length,const online::ByteProgress& progress,const online::SpeedCheck& active){
        if(upload && length>512*1024){++rejectedBodies;throw online::SpeedHttpError(413);}
        return fakeTransfer(http,request,upload,length,progress,active);
    };
    parallelView=SpeedView{};parallelView.phase=SpeedView::Phase::Upload;
    const auto adapted=online::MeasureDirection(parallelRequest,true,parallelView,[](const auto&){},0,0.2,bodyLimited);
    Check(adapted.mbps>0 && rejectedBodies>0 && rejectedBodies<=32,"upload adapts to server body limits with bounded retries");
    Check(online::g_speedUploadLimits[0]<=512*1024,"learned upload limits are reused on subsequent tests");
    online::g_speedUploadLimits[0]=20*1024*1024;
    const auto largerBodies=[&](auto& http,const auto& request,bool upload,size_t length,const auto& progress,const auto& active){
        if(upload && length>8*1024*1024)throw online::SpeedHttpError(413);
        return fakeTransfer(http,request,upload,length,progress,active);
    };
    const auto larger=online::MeasureDirection(parallelRequest,true,parallelView,[](const auto&){},0,0.2,largerBodies);
    Check(larger.mbps>0 && online::g_speedUploadLimits[0]>1024*1024 && online::g_speedUploadLimits[0]<=8*1024*1024,"body-size negotiation does not impose an unrelated 1 MiB ceiling");
    std::atomic<int> refused{0};bool directionBusy=false;
    try{online::MeasureDirection(parallelRequest,false,parallelView,[](const auto&){},0,0.2,[&](auto&,const auto&,bool,size_t length,const auto&,const auto&)->online::SpeedTrial{
        if(length==1000)return {length,0.001,1};++refused;throw online::SpeedBusy(2);
    });}catch(const online::SpeedBusy&){directionBusy=true;}
    Check(directionBusy && refused<=4 && online::SpeedRetrySeconds()>0 && online::g_activeHttp==nullptr,"rate limiting stops the entire transfer group without retries");
    online::g_speedRetryUntil=0;
    Check(commands::Parse(L"/usage",commands::Names{}).kind==commands::Kind::Usage,"hardware command has a local command kind");
    auto usageNames=commands::Resolve({L"weather",L"money",L"crypto",L"ip",L"speedtest",L"\u4F7F\u7528\u60C5\u51B5"});
    Check(commands::Parse(L"/\u4F7F\u7528\u60C5\u51B5",usageNames).kind==commands::Kind::Usage,"hardware command name supports Unicode");
    usageNames.speed=usageNames.usage;
    Check(commands::Parse(L"/\u4F7F\u7528\u60C5\u51B5",usageNames).kind==commands::Kind::Conflict,"hardware command participates in duplicate-name checks");
    std::printf("PASS: %d online card checks\n",checks);
    if (SUCCEEDED(apartment)) RoUninitialize();
}
