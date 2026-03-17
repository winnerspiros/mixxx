#include "controller_mapping_validation_test.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "test/mixxxtest.h"

static std::vector<std::string> SplitMappings(const std::string& s) {
    std::vector<std::string> res;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, '|')) {
        if (!item.empty())
            res.push_back(item);
    }
    return res;
}

class MappingTestFixtureCpp : public MixxxTest, public ::testing::WithParamInterface<std::string> {
};

TEST_P(MappingTestFixtureCpp, ValidateXml) {
    QString path = QString::fromStdString(GetParam());
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));

    QXmlStreamReader xml(&file);
    while (!xml.atEnd()) {
        xml.readNext();
    }
    EXPECT_FALSE(xml.hasError()) << "XML error in " << path.toStdString() << ": " << xml.errorString().toStdString();
}

static std::string PrintMappingName(const ::testing::TestParamInfo<std::string>& info) {
    QFileInfo fileInfo(QString::fromStdString(info.param));
    std::string name = fileInfo.baseName().toStdString();
    std::replace(name.begin(), name.end(), '.', '_');
    std::replace(name.begin(), name.end(), '-', '_');
    return name;
}

#if !defined(__ANDROID__)
#ifdef CONTROLLER_BULK_MAPPINGS
INSTANTIATE_TEST_SUITE_P(BulkMappings,
        MappingTestFixtureCpp,
        ::testing::ValuesIn(SplitMappings(CONTROLLER_BULK_MAPPINGS)),
        PrintMappingName);
#endif

#ifdef CONTROLLER_HID_MAPPINGS
INSTANTIATE_TEST_SUITE_P(HidMappings,
        MappingTestFixtureCpp,
        ::testing::ValuesIn(SplitMappings(CONTROLLER_HID_MAPPINGS)),
        PrintMappingName);
#endif

#ifdef CONTROLLER_MIDI_MAPPINGS
INSTANTIATE_TEST_SUITE_P(MidiMappings,
        MappingTestFixtureCpp,
        ::testing::ValuesIn(SplitMappings(CONTROLLER_MIDI_MAPPINGS)),
        PrintMappingName);
#endif
#endif

#include "moc_controller_mapping_validation_test.cpp"
