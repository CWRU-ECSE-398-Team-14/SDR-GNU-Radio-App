#include <parse_csv.h>

QVector<QVector<QString>> read_csv(std::istream& stream){

    std::string line;
    std::string word;

    QVector<QVector<QString>> csv;

    while(std::getline(stream, line)){
        QVector<QString> words; // this will be our line
        line.push_back(','); //  make sure we read the last word
        std::istringstream line_stream(line);
        while(std::getline(line_stream, word, ',')){
            words.push_back(QString(word.c_str()));
        }
        csv.push_back(words);
    }

    return csv;
}

void write_csv(QVector<QVector<QString>> csv_data, std::ostream& os){
    for(auto line : csv_data){
        int words = line.length();
        int i = 0;
        for(auto word : line){
            os << (word.toStdString());
            i++;
            if(i < words){
                os << ',';
            }else{
                os << '\n';
            }
        }
    }
}


QVector<QVector<QString>> read_csv_file(QString path){
    std::filebuf fbuf;
    QVector<QVector<QString>> retval;
    if(fbuf.open(path.toStdString(), std::ios::in)){
        std::istream is(&fbuf);
        retval = read_csv(is);
    }
    return retval;
}

/**
 * @brief write_csv_file
 * @param csv_data
 * @param path
 */
void write_csv_file(QVector<QVector<QString>> csv_data, QString path){
    std::filebuf fbuf;
    if(fbuf.open(path.toStdString(), std::ios::out)){
        std::ostream os(&fbuf);
        write_csv(csv_data, os);
    }
}


void add_csv_column(QVector<QVector<QString>>& csv_data, QVector<QString>& column){
    if(csv_data.length() != column.length()){
        return;
    }
    for(int i = 0; i < csv_data.length(); i++){
        csv_data[i].append(column[i]);
    }
}
