/**
 * @file configFile.cpp
 * @brief 管理配置文件
 * @version 1.0
 * @date 2020-04-14
 */
#include <QDir>
#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include "configFile.h"

ConfigFile::ConfigFile(const QString &path) :
    configFilePath(path) {
    config_dict.lastPath = "";
    config_dict.fsType = "jffs2";

    QFileInfo fileInfo(configFilePath);
    if(!fileInfo.isFile()) {
        QFile file(configFilePath);
        file.open(QFile::WriteOnly | QFile::Text);
        QXmlStreamWriter writer(&file);
        writer.setAutoFormatting(true); // 自动格式化
        writer.writeStartDocument("1.0");  // 开始文档（XML 声明）
        writer.writeStartElement("config");
        writer.writeTextElement("lastPath", config_dict.lastPath);
        writer.writeTextElement("lastFilePath", config_dict.lastFilePath);
        writer.writeTextElement("fsType", config_dict.fsType);
        writer.writeEndElement();
        writer.writeEndDocument();
        file.close();
    } else {
        QFile file(configFilePath);
        file.open(QFile::ReadOnly | QFile::Text);
        QXmlStreamReader reader(&file);

        while(!reader.atEnd()) {
            if(reader.isStartElement()) {
                if(reader.name().toString() == "lastPath") {
                    config_dict.lastPath = reader.readElementText();
                } else if(reader.name().toString() == "lastFilePath") {
                    config_dict.lastFilePath = reader.readElementText();
                } else if(reader.name().toString() == "fsType") {
                    config_dict.fsType = reader.readElementText();
                }
            }
            reader.readNext();
        }
        file.close();
    }
}

ConfigFile::~ConfigFile() {
    QFile file(configFilePath);
    file.open(QFile::ReadWrite | QFile::Text);
    file.resize(0);

    QXmlStreamWriter writer(&file);
    writer.setAutoFormatting(true); // 自动格式化
    writer.writeStartDocument("1.0");  // 开始文档（XML 声明）
    writer.writeStartElement("config");
    writer.writeTextElement("lastPath", config_dict.lastPath);
    writer.writeTextElement("lastFilePath", config_dict.lastFilePath);
    writer.writeTextElement("fsType", config_dict.fsType);
    writer.writeEndElement();
    writer.writeEndDocument();
    file.close();
}
