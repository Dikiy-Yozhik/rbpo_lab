@echo off
echo ==========================================
echo Запуск SAST-анализа в Docker...
echo ==========================================
echo.

echo [1/3] Запуск Cppcheck...
docker run --rm -v "%cd%:/workspace" jpeg_sast sh -c "cd /workspace && cppcheck --enable=all --inconclusive --std=c++17 -I /usr/local/include main.cpp > cppcheck_report.txt 2>&1"
echo [+] Cppcheck завершен. Отчет: cppcheck_report.txt
echo.

echo [2/3] Запуск Clang Static Analyzer (scan-build)...
docker run --rm -v "%cd%:/workspace" jpeg_sast sh -c "cd /app/build && rm -rf * && scan-build cmake .. && scan-build -o /workspace/clang_report make"
echo [+] Clang SA завершен. Отчет в папке: clang_report (открой index.html)
echo.

echo [3/3] Запуск Flawfinder...
docker run --rm -v "%cd%:/workspace" jpeg_sast sh -c "cd /workspace && flawfinder --html --minlevel=1 main.cpp > flawfinder_report.html"
echo [+] Flawfinder завершен. Отчет: flawfinder_report.html
echo.

echo ==========================================
echo Все SAST-сканирования успешно завершены!
echo ==========================================
pause