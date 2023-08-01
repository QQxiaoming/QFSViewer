/**
 * @file configFile.h
 * @brief 管理配置文件
 * @version 1.0
 * @date 2020-04-14
 */
#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include <QString>

class ConfigFile {
public:
    ConfigFile(const QString &path = QString());
    ~ConfigFile();

    typedef struct {
        QString lastPath;
        QString fsType;
    }config_dict_t;
    config_dict_t config_dict;

private:
    QString configFilePath;
};

#endif // CONFIGFILE_H