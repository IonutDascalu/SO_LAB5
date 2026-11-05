#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>

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

int main() {
    const int NUM_PROCESE = 10;
    const int PAS = 1000;

    int pipe_parent_to_child[NUM_PROCESE][2];
    int pipe_child_to_parent[NUM_PROCESE][2];
    pid_t pids[NUM_PROCESE];

    for (int i = 0; i < NUM_PROCESE; ++i) {
        if (pipe(pipe_parent_to_child[i]) == -1 || pipe(pipe_child_to_parent[i]) == -1) {
            std::cerr << "Eroare la crearea pipe-urilor!\n";
            return 1;
        }

        pids[i] = fork();

        if (pids[i] < 0) {
            std::cerr << "Eroare la fork!\n";
            return 1;
        }

        if (pids[i] == 0) {
            // --- COD COPIL ---
            close(pipe_parent_to_child[i][1]);
            close(pipe_child_to_parent[i][0]);

            Interval itv;
            read(pipe_parent_to_child[i][0], &itv, sizeof(Interval));
            close(pipe_parent_to_child[i][0]);

            for (int num = itv.start; num <= itv.end; ++num) {
                if (estePrim(num)) {
                    write(pipe_child_to_parent[i][1], &num, sizeof(int));
                }
            }

            int stop_signal = -1;
            write(pipe_child_to_parent[i][1], &stop_signal, sizeof(int));

            close(pipe_child_to_parent[i][1]);
            return 0;
        } else {
            // --- COD PARINTE ---
            close(pipe_parent_to_child[i][0]);
            close(pipe_child_to_parent[i][1]);
        }
    }

    // Trimitem intervalele catre copii
    for (int i = 0; i < NUM_PROCESE; ++i) {
        Interval itv = { i * PAS + 1, (i + 1) * PAS };
        write(pipe_parent_to_child[i][1], &itv, sizeof(Interval));
        close(pipe_parent_to_child[i][1]);
    }

    // Citim si afisam rezultatele de la copii
    for (int i = 0; i < NUM_PROCESE; ++i) {
        int val;
        std::cout << "--- Numere prime din intervalul [" 
                  << (i * PAS + 1) << " - " << ((i + 1) * PAS) << "] ---\n";
        
        while (read(pipe_child_to_parent[i][0], &val, sizeof(int)) > 0) {
            if (val == -1) break;
            std::cout << val << " ";
        }
        std::cout << "\n\n";
        close(pipe_child_to_parent[i][0]);
    }

    for (int i = 0; i < NUM_PROCESE; ++i) {
        waitpid(pids[i], NULL, 0);
    }

    return 0;
}