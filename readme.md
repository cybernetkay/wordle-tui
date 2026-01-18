# Wordle C

A minimal, POSIX-compliant implementation of Wordle for the terminal, written in C99.
It does not depend on ncurses or other external libraries. It uses standard ANSI escape codes for rendering and `<termios.h>` for raw input handling.

## Building

To build the project simply run:

    make

To run it:

    ./wordle

Or simply:

    make run

## Dependencies

- A C99 compiler (GCC or Clang)
- A POSIX environment (Linux, macOS, WSL)
- `make` (optional)

## Implementation details

The game runs on a simple state machine loop handling input and rendering:

- **Input**: The terminal is switched to raw mode (disabling canonical mode and echo) to handle keystrokes immediately without waiting for newline.
- **Rendering**: The grid is centered dynamically using `ioctl` to fetch terminal dimensions at every frame.
- **Logic**: The coloring algorithm uses a two-pass approach (Green first, then Yellow) to correctly handle duplicate letters and priority.

--------------------------------------------------------------------------------

# Wordle C (Italiano)

Implementazione minimale e POSIX-compliant di Wordle per terminale, scritta in C99.
Non dipende da ncurses o librerie esterne. Utilizza codici di escape ANSI standard per il rendering e `<termios.h>` per la gestione dell'input raw.

## Compilazione

Per compilare il progetto eseguire:

    make

Per avviarlo:

    ./wordle

Oppure semplicemente:

    make run

## Dipendenze

- Un compilatore C99 (GCC o Clang)
- Ambiente POSIX (Linux, macOS, WSL)
- `make` (opzionale)

## Dettagli implementativi

Il gioco utilizza un loop a macchina a stati che gestisce input e rendering:

- **Input**: Il terminale viene impostato in raw mode (disabilitando la modalità canonica e l'echo) per gestire la pressione dei tasti immediatamente.
- **Rendering**: La griglia viene centrata dinamicamente usando `ioctl` per ottenere le dimensioni del terminale ad ogni frame.
- **Logica**: L'algoritmo di colorazione usa un approccio a due passaggi (prima i Verdi, poi i Gialli) per gestire correttamente le lettere doppie e la priorità dei colori.
