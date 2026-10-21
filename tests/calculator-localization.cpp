// Test driver; scripts/test-localization.mjs supplies the production code.
unsigned checks=0;void Check(bool pass,const char* label){++checks;if(!pass){printf("FAIL %s\n",label);ExitProcess(1);}}
bool Near(double a,double b){return std::abs(a-b)<=6e-6*std::max(1.0,std::abs(b));}
void TestLocalizationAndSymbols() {
    for (const auto& locale : i18n::kLocales) {
        i18n::Load(locale.tag);
        Check(!wcscmp(i18n::Tag(), locale.tag), "every language option resolves to its own pack");
        for (size_t key=0; key<static_cast<size_t>(i18n::Key::Count); ++key) {
            Check(i18n::Text(static_cast<i18n::Key>(key))[0]!=0, "every visible string exists in every pack");
            Check(i18n::Label(i18n::kLocales[0].text[key])==locale.text[key], "dynamic labels use the selected pack");
        }
        const std::wstring name=L"App {1} \\ % 你好";
        const auto message=i18n::Format(i18n::Key::UninstallPrompt,{name});
        Check(message.find(name)!=std::wstring::npos, "formatting preserves user names and literal braces");
    }
    for (auto [requested,expected] : std::vector<std::pair<std::wstring,std::wstring>>{
        {L"EN_us",L"en"},{L"de-DE",L"de"},{L"fr-CA",L"fr"},{L"pt-BR",L"pt-br"},{L"pt-PT",L"pt-pt"},
        {L"zh-CN",L"zh-hans"},{L"zh-SG",L"zh-hans"},{L"zh-TW",L"zh-hant"},{L"zh-HK",L"zh-hant"},
        {L"zh-Hant-HK",L"zh-hant"},{L"zh-Hans-CN",L"zh-hans"},{L"sr-Latn-RS",L"sr-latn"},
        {L"sr-Cyrl-RS",L"en"},{L"no-NO",L"nb"},{L"tl-PH",L"fil"},{L"ar-IQ",L"en"},
        {L"he-IL",L"en"},{L"fa-IR",L"en"},{L"ur-PK",L"en"},{L"unknown",L"en"}}) {
        i18n::Load(requested);
        Check(i18n::Tag()==expected, "regional/script variants and unsupported or RTL locales resolve safely");
    }
    i18n::Load(std::wstring(1000,L'a'));
    Check(!wcscmp(i18n::Tag(),L"en"), "overlong locale cannot select an invalid pack");
    i18n::Load(L"en");
    Check(i18n::Format(i18n::Key::SearchService,{L"Engine",L"{0} 你好"})==L"Search Engine for “{0} 你好”",
          "format values are never interpreted as template placeholders");
    Check(i18n::Subtitle(L"Settings \u2022 Windows Update")==L"Settings \u2022 Windows Update",
          "native Settings names remain untouched");
    i18n::Load(L"de");
    Check(i18n::Subtitle(L"Settings \u2022 Windows Update")==L"Einstellungen \u2022 Windows Update",
          "translate only the mod-owned Settings label");
    Check(i18n::Subtitle(L"C:\\Apps\\Settings.exe")==L"C:\\Apps\\Settings.exe",
          "never translate paths");
    std::atomic<bool> good{true};
    std::thread writer([] { for(int i=0;i<3000;++i) i18n::Load(i&1?L"de":L"ja"); });
    for (int i=0;i<3000;++i) {
        const std::wstring text=i18n::Text(i18n::Key::Apps);
        if(text!=L"Apps" && text!=L"アプリ") good=false;
        const auto formatted=i18n::Format(i18n::Key::OpenService,{L"sample"});
        if(formatted!=L"sample öffnen" && formatted!=L"sample を開く") good=false;
    }
    writer.join();
    Check(good, "language changes publish a complete immutable pack across threads");
    i18n::Load(L"en");
    for (auto [expression, expected] : std::vector<std::pair<std::wstring,double>>{
        {L"10% * 7",0.7},{L"7 * 10%",0.7},{L"10% × 7",0.7},{L"(5+5)% * 7",0.7},
        {L"10%",0.1},{L"200*(1+10%)",220},{L"10%+7",7.1},{L"10 % 3",1},{L"10 % (3)",1},
        {L"8 ÷ 2",4},{L"5 − 2",3},{L"2 · 3",6},{L"√(144)",12},{L"2 * π",6.283185307179586},
        {L"1,5 * 2",3},{L",5 * 2",1},{L"2,5e2 * 10%",25},{L"-10% * 7",-0.7},
        {L"2^10%",std::pow(2.0,0.1)},{L"(2^10)%",10.24},{L"15% of 200",30}}) {
        double result=0;
        Check(tools::EvaluateMath(expression,result)&&Near(result,expected), "symbol calculator returns the expected value");
        const auto rows=calc::Rows(expression);
        Check(!rows.empty()&&calc::ParseNumber(rows.front().copyText,&result)&&Near(result,expected),
              "real calculator rows expose and copy the symbolic result");
    }
    for (auto expression : {L"1.2.3+4",L"1,2,3+4",L"1e+ +2",L"10%%",L"%7",L"10%*",L"√(-1)",L"7÷0"}) {
        double result=0;
        Check(!tools::EvaluateMath(expression,result), "reject malformed and nonfinite symbolic expressions");
    }
    const auto converted=calc::Rows(L"100 km → mi");
    double miles=0;
    Check(converted.size()==1&&calc::ParseNumber(converted[0].copyText,&miles)&&Near(miles,62.1371192),
          "unit conversion accepts a symbolic arrow");
    i18n::Load(L"auto");
    Check(i18n::Text(i18n::Key::Apps)[0], "automatic Windows language selection succeeds");
}

int main(){SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);setvbuf(stdout,nullptr,_IONBF,0);double result=0;TestLocalizationAndSymbols();
for(auto [expression,expected]:std::vector<std::pair<std::wstring,double>>{{L"2+2",4},{L"2^3^2",512},{L"50% of 200",100},{L"sqrt(9)+abs(-2)",5},{L"0xFF+1",256},{L"(1+2)*3",9},{L"10% of 50% of 200",10}})Check(tools::EvaluateMath(expression,result)&&Near(result,expected),"normal calculation preserves its result");
for(auto expression:{L"1/0",L"sqrt(-1)",L"log(0)",L"2^^2",L"unknown(3)",L"sin()",L"100% of 1e9999"}){bool accepted=tools::EvaluateMath(expression,result);if(accepted)printf("ACCEPTED invalid=%ls value=%g\n",expression,result);Check(!accepted,"invalid or nonfinite calculation rejected");}
for(unsigned count:{64u,1000u,10000u}){Check(!tools::EvaluateMath(std::wstring(count,L'(')+L"1"+std::wstring(count,L')'),result),"deep parentheses rejected without a crash");Check(tools::EvaluateMath(L"2+2",result)&&result==4,"depth resets after rejected input");}
Check(tools::EvaluateMath(std::wstring(20,L'(')+L"1"+std::wstring(20,L')'),result)&&result==1,"ordinary nesting remains supported");
for(auto mode:{0,1,2}){std::wstring expr;if(mode==0)expr=std::wstring(20000,L'-')+L"1";if(mode==1){for(int i=0;i<10000;i++)expr+=L"1^";expr+=L"1";}if(mode==2){for(int i=0;i<1000;i++)expr+=L"1% of ";expr+=L"1";}Check(!tools::EvaluateMath(expr,result),"very long recursive syntax rejected");}
std::wstring flat=L"1";for(int i=1;i<1000;i++)flat+=L"+1";Check(tools::EvaluateMath(flat,result)&&result==1000,"long flat sums remain valid");
std::wstring nestedPct;for(int i=0;i<80;i++)nestedPct+=L"1% of ";nestedPct+=L"1";Check(!tools::EvaluateMath(nestedPct,result),"percentage recursion bounded below the text length limit");Check(tools::EvaluateMath(L"3*7",result)&&result==21,"percentage depth unwinds correctly");
for(auto name:{L"2024 report",L"3d",L"4k",L"5g",L"2024-01-01",L"1920x1080",L"1000",L"?test",L"/ip"})Check(calc::Rows(name).empty(),"ordinary names and other providers stay out of calculator");
auto mathRows=calc::Rows(L"2+2");Check(!mathRows.empty()&&mathRows.front().copyText==L"4","calculator row copies its numeric result");
auto units=calc::Rows(L"100 km to mi");Check(units.size()==1&&calc::ParseNumber(units[0].copyText,&result)&&Near(result,62.1371192),"explicit length conversion");
auto temp=calc::Rows(L"0 C to F");Check(temp.size()==1&&calc::ParseNumber(temp[0].copyText,&result)&&Near(result,32),"temperature offsets preserved");
printf("STAGE basic calculations passed\n");unsigned conversionChecks=0;for(auto const& from:calc::kUnits)for(auto const& to:calc::kUnits){if(wcscmp(from.kind,to.kind)||calc::FindUnit(from.symbol)!=&from||calc::FindUnit(to.symbol)!=&to)continue;for(double n:{0.0,1.0,-12.5,1000.0}){std::vector<Row> rows;Check(calc::ConversionRows(n,std::wstring(from.symbol)+L" to "+to.symbol,{},&rows)&&!rows.empty(),"built-in conversion produces a row");double parsed=0;double expected=(n*from.factor+from.offset-to.offset)/to.factor;{bool parsedOk=calc::ParseNumber(rows.back().copyText,&parsed)!=0;if(!parsedOk||!Near(parsed,expected))printf("CONVERSION n=%g expected=%.15g got=%.15g from=%ls to=%ls text=%ls\n",n,expected,parsed,from.symbol,to.symbol,rows.back().copyText.c_str());Check(parsedOk&&Near(parsed,expected),"built-in conversion has correct numeric value");}++conversionChecks;}}
printf("STAGE built-in conversions complete\n");g_settings.unitConversions={{L"smoot",L"m",L"{n}*1.7018",L"Length",true}};auto custom=calc::Rows(L"2 smoot");bool customGood=false;for(auto const& row:custom)if(row.copyText.find(L"m")!=std::wstring::npos&&calc::ParseNumber(row.copyText,&result)&&Near(result,3.4036))customGood=true;Check(customGood,"custom formula conversion still works");
printf("STAGE custom conversion complete\n");std::mt19937 rng(0x5345);const wchar_t* alphabet=L"0123456789+-*/^().% abcdef,x to nms";for(int i=0;i<10000;i++){std::wstring query;const auto n=rng()%128;for(unsigned j=0;j<n;j++)query+=alphabet[rng()%wcslen(alphabet)];tools::EvaluateMath(query,result);calc::Rows(query);}Check(true,"ten thousand malformed mixed queries complete without a crash");
std::atomic<bool> parallelOk{true};std::vector<std::thread> threads;for(int i=0;i<4;i++)threads.emplace_back([&]{for(int j=0;j<1000;j++){double v;tools::EvaluateMath(nestedPct,v);if(!tools::EvaluateMath(L"6*7",v)||v!=42)parallelOk=false;}});for(auto& thread:threads)thread.join();Check(parallelOk,"parser limits remain independent across threads");printf("RESULT %u utility checks passed; %u numeric conversion cases; 10000 malformed queries\n",checks,conversionChecks);}
