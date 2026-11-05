#include <iostream>
#include <windows.h>
#include <string>
#include <vector>

bool estePrim(int n) {
    if (n < 2) return false;
    for (int d = 2; d * d <= n; ++d) {
        if (n % d == 0) return false;
    }
    return true;
}

struct Interval {
    int start;
    int end;
};

void rulaModCopil(HANDLE hRead, HANDLE hWrite) {
    Interval itv;
    DWORD bytesRead, bytesWritten;

    // Citim intervalul primit de la parinte
    if (ReadFile(hRead, &itv, sizeof(Interval), &bytesRead, NULL) && bytesRead > 0) {
        for (int num = itv.start; num <= itv.end; ++num) {
            if (estePrim(num)) {
                WriteFile(hWrite, &num, sizeof(int), &bytesWritten, NULL);
            }
        }
    }

    // Trimitem semnalul de final (-1)
    int stop_signal = -1;
    WriteFile(hWrite, &stop_signal, sizeof(int), &bytesWritten, NULL);

    CloseHandle(hRead);
    CloseHandle(hWrite);
}

int main(int argc, char* argv[]) {
    // Daca programul a fost lansat cu argumentul "child", el ruleaza ca proces copil
    if (argc == 4 && std::string(argv[1]) == "child") {
        HANDLE hRead = (HANDLE)(uintptr_t)std::stoull(argv[2]);
        HANDLE hWrite = (HANDLE)(uintptr_t)std::stoull(argv[3]);
        rulaModCopil(hRead, hWrite);
        return 0;
    }

    // --- COD PROCES PARINTE ---
    const int NUM_PROCESE = 10;
    const int PAS = 1000;

    // Obținem calea catre executabilul curent pentru CreateProcess
    char szPath[MAX_PATH];
    GetModuleFileNameA(NULL, szPath, MAX_PATH);

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE; // Permitem mostenirea handle-urilor
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hParentWrite[NUM_PROCESE];
    HANDLE hParentRead[NUM_PROCESE];
    PROCESS_INFORMATION pi[NUM_PROCESE];

    for (int i = 0; i < NUM_PROCESE; ++i) {
        HANDLE hChildRead, hChildWrite;

        // Pipe Parinte -> Copil
        CreatePipe(&hChildRead, &hParentWrite[i], &saAttr, 0);
        SetHandleInformation(hParentWrite[i], HANDLE_FLAG_INHERIT, 0); // Parintele nu mosteneste capatul de scriere

        // Pipe Copil -> Parinte
        CreatePipe(&hParentRead[i], &hChildWrite, &saAttr, 0);
        SetHandleInformation(hParentRead[i], HANDLE_FLAG_INHERIT, 0); // Parintele nu mosteneste capatul de citire

        // Construim linia de comanda pentru procesul copil
        std::string cmd = std::string("\"") + szPath + "\" child " +
                          std::to_string((uintptr_t)hChildRead) + " " +
                          std::to_string((uintptr_t)hChildWrite);

        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi[i], sizeof(PROCESS_INFORMATION));

        // Creăm procesul copil
        BOOL success = CreateProcessA(
            NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi[i]
        );

        if (!success) {
            std::cerr << "Eroare la CreateProcess pentru copilul " << i << "\n";
            return 1;
        }

        // Inchidem handle-urile folosite doar de copil in parinte
        CloseHandle(hChildRead);
        CloseHandle(hChildWrite);
    }

    // --- PARINTE: Trimitem intervalele ---
    for (int i = 0; i < NUM_PROCESE; ++i) {
        Interval itv = { i * PAS + 1, (i + 1) * PAS };
        DWORD bytesWritten;
        WriteFile(hParentWrite[i], &itv, sizeof(Interval), &bytesWritten, NULL);
        CloseHandle(hParentWrite[i]); // Inchidem pipe-ul de scriere
    }

    // --- PARINTE: Citim si afisam rezultatele ---
    for (int i = 0; i < NUM_PROCESE; ++i) {
        int val;
        DWORD bytesRead;
        std::cout << "--- Numere prime din intervalul [" 
                  << (i * PAS + 1) << " - " << ((i + 1) * PAS) << "] ---\n";

        while (ReadFile(hParentRead[i], &val, sizeof(int), &bytesRead, NULL) && bytesRead > 0) {
            if (val == -1) break;
            std::cout << val << " ";
        }
        std::cout << "\n\n";
        CloseHandle(hParentRead[i]);
    }

    // Așteptăm ca toate procesele să se încheie
    for (int i = 0; i < NUM_PROCESE; ++i) {
        WaitForSingleObject(pi[i].hProcess, INFINITE);
        CloseHandle(pi[i].hProcess);
        CloseHandle(pi[i].hThread);
    }

    return 0;
}