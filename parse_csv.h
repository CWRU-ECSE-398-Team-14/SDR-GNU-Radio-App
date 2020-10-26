#ifndef PARSE_CSV_H
#define PARSE_CSV_H

#include <QVector>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>

QVector<QVector<QString>> read_csv(std::istream& stream);

void write_csv(QVector<QVector<QString>> csv_data, std::istream stream);

#endif // PARSE_CSV_H
