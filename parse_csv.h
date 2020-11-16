#ifndef PARSE_CSV_H
#define PARSE_CSV_H

#include <QVector>
#include <QDebug>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>

QVector<QVector<QString>> read_csv(std::istream& stream);

void write_csv(QVector<QVector<QString>> csv_data, std::istream& stream);

QVector<QVector<QString>> read_csv_file(QString path);

void write_csv_file(QVector<QVector<QString>> csv_data, QString path);

void add_csv_column(QVector<QVector<QString>>& csv_data, QVector<QString>& column);

#endif // PARSE_CSV_H
