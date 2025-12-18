#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <cstddef>

// Lê do socket até encontrar '\n' ou EOF.
// Pode devolver string vazia em caso de erro/EOF antes de qualquer byte.
std::string recv_line(int fd);

// Lê exatamente n bytes (a não ser que haja erro/EOF). 
// devolve true se conseguiu ler tudo, false se erro.
bool read_exact(int fd, void *buf, std::size_t n);

// Escreve exatamente n bytes (a não ser que haja erro).
bool write_exact(int fd, const void *buf, std::size_t n);

// Devolve true se o ficheiro existir e for regular.
bool file_exists(const std::string &path);

// Lê a primeira linha do ficheiro, devolvendo true se conseguiu ler alguma coisa.
bool read_first_line(const std::string &path, std::string &line_out);

#endif