#include <iostream>
#include <cstdlib>
#include <limits>
#include <optional>
#include <cassert>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>

#define BUILDER_DECL
#include <docbuilder.h>

const std::string testDirectory = std::string(CMAKE_SOURCE_DIR) + "/test";
const std::string inputPath = testDirectory + '/' +
    "test2.odp";
const std::string outputFileName =
    "output";
const std::string outputExtension =
    "pdf";
const std::string outputPath = testDirectory + '/' + outputFileName + '.' + outputExtension;

std::optional<std::wstring> wideFromMultiByte(const std::string& multiByte)
{
    std::wstring wide;
    size_t wideLength = std::mbstowcs(nullptr, multiByte.c_str(), std::numeric_limits<size_t>::max());
    if (wideLength == static_cast<size_t>(-1)) {
        return std::nullopt;
    }
    else
    {
        wide.reserve(wideLength + 1);
        std::mbstowcs(&wide[0], multiByte.c_str(), wide.capacity());
        return wide;
    }
}

namespace bpo = boost::program_options;
namespace bfs = boost::filesystem;
namespace docr = NSDoctRenderer;


template<typename T>
std::optional<T> tryGetOptionAs(const bpo::variables_map& options, const std::string& optionName)
{
    if (auto found = options.find(optionName); found != std::end(options)) {
        return found->second.as<T>();
    }
    else {
        return std::nullopt;
    }
}

int main(int argc, char* argv[])
{
    bpo::options_description visibleOptions("Options");
    visibleOptions.add_options()
        ("output-file", bpo::value<std::string>()->value_name("path"), "path to output file")
        ("resolution", bpo::value<std::string>()->value_name("WxH")->default_value("1920x1080"), "resolution")
        ("stretch", "stretch to fit resolution")
        ("only-first-page-image", "only first page will be converted to image");
    bpo::options_description options;
    options.add_options()
        ("format", bpo::value<std::string>(), "output format")
        ("input-file", bpo::value<std::string>(), "path to input file");
    bpo::positional_options_description positionalOptions;
    positionalOptions.add("format", 1); positionalOptions.add("input-file", 2);

    bpo::variables_map optionValues;
    constexpr int invalidArgumentErrorReturnCode = 1;
    try {
        bpo::store(bpo::command_line_parser(argc, argv).options(options.add(visibleOptions)).positional(positionalOptions).run(), optionValues);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return invalidArgumentErrorReturnCode;
    }

    if (argc < 2 || optionValues.empty())
    {
        std::cout << "Usage: " << "ooconv format input-file [options]" << std::endl;
        std::cout << visibleOptions << std::endl;
        return 0;
    }

    //Prepare arguments.
    auto format = tryGetOptionAs<std::string>(optionValues, "format");
    if (!format)
    {
        std::cerr << "Output format must be provided" << std::endl;
        return invalidArgumentErrorReturnCode;
    }
    auto inputFile = tryGetOptionAs<std::string>(optionValues, "input-file");
    if (!inputFile)
    {
        std::cerr << "Path to input file must be provided" << std::endl;
        return invalidArgumentErrorReturnCode;
    }
    assert(format);
    auto wformat = wideFromMultiByte(*format);
    if (!wformat)
    {
        std::cerr << "Unable to transform format to required internal type" << std::endl;
        return invalidArgumentErrorReturnCode;
    }

    assert(tryGetOptionAs<std::string>(optionValues, "resolution"));
    std::string resolution = *tryGetOptionAs<std::string>(optionValues, "resolution");
    int width = 0, height = 0;
    try {
        auto separatorPos = resolution.find("x", 0);
        if (separatorPos == decltype(resolution)::npos) {
            throw std::invalid_argument("");
        }
        width = std::stoi(resolution.substr(0, separatorPos));
        height = std::stoi(resolution.substr(separatorPos + 1));
        if (width < 0 || height < 0) {
            throw std::out_of_range("");
        }
    }
    catch (const std::invalid_argument&)
    {
        std::cerr << "Invalid resolution format." << std::endl;
        return invalidArgumentErrorReturnCode;
    }
    catch (const std::out_of_range&)
    {
        std::cerr << "Resolution is out of possible range." << std::endl;
        return invalidArgumentErrorReturnCode;
    }

    namespace dr = NSDoctRenderer;
    assert(inputFile);
    bfs::path inputFilePath(*inputFile);
    bfs::path outputFilePath;
    if (auto outputFile = tryGetOptionAs<std::string>(optionValues, "output-file")) {
        outputFilePath = *std::move(outputFile);
    }
    else
    {
        outputFilePath = inputFilePath;
        outputFilePath.replace_extension(*format);
    }

    dr::CDocBuilder::Initialize(wideFromMultiByte(DOCUMENT_BUILDER_WORKING_DIRECTORY)->c_str());
    dr::CDocBuilder builder;
    constexpr int drErrorReturnCode = invalidArgumentErrorReturnCode + 1;
    if (int error = builder.OpenFile(inputFilePath.generic_wstring().c_str(), L""))
    {
        std::cerr << "Error opening file. Code:" << error << std::endl;
        return drErrorReturnCode;
    }

    std::wstring wparams;
    const std::wstring formatJpg = std::to_wstring(3), formatPng = std::to_wstring(4);
    std::wstring wImageFormat =
        boost::iequals(*format, "jpg") ? formatJpg :
        (boost::iequals(*format, "png") ? formatPng : L"");
    if (!wImageFormat.empty())
    {
        //<m_oThumbnail><format>%format</format><aspect>%aspect</aspect><first>%first</first><width>%width</width><height>%h</height></m_oThumbnail>
        const std::wstring stretch = std::to_wstring(0), preserve = std::to_wstring(1);
        wparams =
            std::wstring(L"<m_oThumbnail>") +
            L"<format>" + wImageFormat + L"</format>"
            L"<aspect>" + (optionValues.count("stretch") ? stretch : preserve) + L"</aspect>"
            L"<first>" + (optionValues.count("only-first-page-image") ? L"true" : L"false") + L"</first>"
            L"<width>" + std::to_wstring(width) + L"</width>"
            L"<height>" + std::to_wstring(height) + L"</height>"
            L"</m_oThumbnail>";
    }

    if (int error = builder.SaveFile(wformat->c_str(), outputFilePath.generic_wstring().c_str(), !wparams.empty() ? wparams.c_str() : nullptr))
    {
        std::cerr << "Error saving file. Code:" << error << std::endl;
        return drErrorReturnCode;
    }
    builder.CloseFile();

    //if (int error = builder.OpenFile(bfs::path(inputPath).generic_wstring().c_str(), L""))
    //{
    //    std::cerr << "Error opening file. Code:" << error << std::endl;
    //    return drErrorReturnCode;
    //}
    //if (int error = builder.SaveFile(wideFromMultiByte(outputExtension)->c_str(), bfs::path(outputPath).generic_wstring().c_str()))
    //{
    //    std::cerr << "Error saving file. Code:" << error << std::endl;
    //    return drErrorReturnCode;
    //}
    //builder.CloseFile();


    return 0;
}