set mypath=%cd%
cmd /k "cd \Program Files (x86)\Microsoft Visual Studio 14.0\VC & vcvarsall amd64 & cd /d %mypath% & cl /EHsc /openmp /O2 /I ../lib/rapidjson/include/rapidjson user32.lib  odbc32.lib riskMeasuremain.cpp /I ../lib/eigen /I ../Complex /I ../RungeKutta /I ../FangOostserlee & exit 0"