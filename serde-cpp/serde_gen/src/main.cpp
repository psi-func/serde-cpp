#include <iostream>
#include <fstream>
#include <filesystem>

#include <cxxopts.hpp>

#include <cppast/code_generator.hpp>         // for generate_code()
#include <cppast/cpp_entity_kind.hpp>        // for the cpp_entity_kind definition
#include <cppast/cpp_forward_declarable.hpp> // for is_definition()
#include <cppast/cpp_namespace.hpp>          // for cpp_namespace
#include <cppast/libclang_parser.hpp> // for libclang_parser, libclang_compile_config, cpp_entity,...
#include <cppast/cpp_member_variable.hpp>
#include <cppast/visitor.hpp>         // for visit()

// prints the AST entry of a cpp_entity (base class for all entities),
// will only print a single line
void print_entity(std::ostream& out, const cppast::cpp_entity& e)
{
    // print name and the kind of the entity
    if (!e.name().empty())
        out << e.name();
    else
        out << "<anonymous>";
    out << " (" << cppast::to_string(e.kind()) << ")";

    // print whether or not it is a definition
    if (cppast::is_definition(e))
        out << " [definition]";

    // print number of attributes
    if (!e.attributes().empty())
        out << " [" << e.attributes().size() << " attribute(s)]";

    if (e.kind() == cppast::cpp_entity_kind::language_linkage_t)
        // no need to print additional information for language linkages
        out << '\n';
    else if (e.kind() == cppast::cpp_entity_kind::namespace_t)
    {
        // cast to cpp_namespace
        auto& ns = static_cast<const cppast::cpp_namespace&>(e);
        // print whether or not it is inline
        if (ns.is_inline())
            out << " [inline]";
        out << '\n';
    }
    else
    {
        // print the declaration of the entity
        // it will only use a single line
        // derive from code_generator and implement various callbacks for printing
        // it will print into a std::string
        class code_generator : public cppast::code_generator
        {
            std::string str_;                 // the result
            bool        was_newline_ = false; // whether or not the last token was a newline
            // needed for lazily printing them

        public:
            code_generator(const cppast::cpp_entity& e)
            {
                // kickoff code generation here
                cppast::generate_code(*this, e);
            }

            // return the result
            const std::string& str() const noexcept
            {
                return str_;
            }

        private:
            // called to retrieve the generation options of an entity
            generation_options do_get_options(const cppast::cpp_entity&,
                                              cppast::cpp_access_specifier_kind) override
            {
                // generate declaration only
                return code_generator::declaration;
            }

            // no need to handle indentation, as only a single line is used
            void do_indent() override {}
            void do_unindent() override {}

            // called when a generic token sequence should be generated
            // there are specialized callbacks for various token kinds,
            // to e.g. implement syntax highlighting
            void do_write_token_seq(cppast::string_view tokens) override
            {
                if (was_newline_)
                {
                    // lazily append newline as space
                    str_ += ' ';
                    was_newline_ = false;
                }
                // append tokens
                str_ += tokens.c_str();
            }

            // called when a newline should be generated
            // we're lazy as it will always generate a trailing newline,
            // we don't want
            void do_write_newline() override
            {
                was_newline_ = true;
            }

        } generator(e);
        // print generated code
        out << ": `" << generator.str() << '`' << '\n';
    }
}

// prints the AST of a file
void print_ast(std::ostream& out, const cppast::cpp_file& file)
{
    // print file name
    out << "AST for '" << file.name() << "':\n";
    std::string prefix; // the current prefix string
    // recursively visit file and all children
    cppast::visit(file, [&](const cppast::cpp_entity& e, cppast::visitor_info info) {
        if (e.kind() == cppast::cpp_entity_kind::file_t || cppast::is_templated(e)
            || cppast::is_friended(e))
            // no need to do anything for a file,
            // templated and friended entities are just proxies, so skip those as well
            // return true to continue visit for children
            return true;
        else if (info.event == cppast::visitor_info::container_entity_exit)
        {
            // we have visited all children of a container,
            // remove prefix
            prefix.pop_back();
            prefix.pop_back();
        }
        else
        {
            out << prefix; // print prefix for previous entities
            // calculate next prefix
            if (info.last_child)
            {
                if (info.event == cppast::visitor_info::container_entity_enter)
                    prefix += "  ";
                out << "+-";
            }
            else
            {
                if (info.event == cppast::visitor_info::container_entity_enter)
                    prefix += "| ";
                out << "|-";
            }

            print_entity(out, e);
        }

        return true;
    });
}

std::unique_ptr<cppast::cpp_file>
parse_file(const cppast::libclang_compile_config &config,
           const cppast::diagnostic_logger &logger, const std::string &filename, bool fatal_error)
{
  // the entity index is used to resolve cross references in the AST
  // we don't need that, so it will not be needed afterwards
  cppast::cpp_entity_index idx;
  // the parser is used to parse the entity
  // there can be multiple parser implementations
  cppast::libclang_parser parser(type_safe::ref(logger));
  // parse the file
  auto file = parser.parse(idx, filename, config);
  if (fatal_error && parser.error())
    return nullptr;
  return file;
}

void generate_serde(std::ofstream& outfile, const cppast::cpp_file& file)
{
    outfile << R"(/* Auto-generated serde-cpp file! DO NOT EDIT!!! */

#include <string>
#include <serde/serde.h>
#include <serde/std/string.h>
)";

    cppast::visit(file,
        [](const cppast::cpp_entity& e) {
            // only visit non-templated class definitions that have the attribute set
            return (e.kind() == cppast::cpp_entity_kind::class_t
                    && cppast::is_definition(e) && cppast::has_attribute(e, "serde"))
                   // or all namespaces
                   || e.kind() == cppast::cpp_entity_kind::namespace_t;
        },
        [&](const cppast::cpp_entity& e, cppast::visitor_info info) {
            if (e.kind() == cppast::cpp_entity_kind::class_t && !info.is_old_entity())
            {
            auto& class_ = static_cast<const cppast::cpp_class&>(e);

            // print the declaration of the entity
            // it will only use a single line
            // derive from code_generator and implement various callbacks for printing
            // it will print into a std::string
            class code_generator : public cppast::code_generator
            {
            std::string str_;                 // the result
            bool        was_newline_ = false; // whether or not the last token was a newline
                                              // needed for lazily printing them

              public:
            code_generator(const cppast::cpp_entity& e)
            {
              // kickoff code generation here
              cppast::generate_code(*this, e);
            }

            // return the result
            const std::string& str() const noexcept
            {
              return str_;
            }

              private:
            // called to retrieve the generation options of an entity
            generation_options do_get_options(const cppast::cpp_entity&,
                cppast::cpp_access_specifier_kind) override
            {
              // generate declaration only
              return code_generator::declaration;
            }

            // no need to handle indentation, as only a single line is used
            void do_indent() override {}
            void do_unindent() override {}

            // called when a generic token sequence should be generated
            // there are specialized callbacks for various token kinds,
            // to e.g. implement syntax highlighting
            void do_write_token_seq(cppast::string_view tokens) override
            {
              if (was_newline_)
              {
                // lazily append newline as space
                str_ += ' ';
                was_newline_ = false;
              }
              // append tokens
              str_ += tokens.c_str();
            }

            // called when a newline should be generated
            // we're lazy as it will always generate a trailing newline,
            // we don't want
            void do_write_newline() override
            {
              was_newline_ = true;
            }

            } generator(e);

            outfile << "\n" << generator.str() << "\n";

            outfile << R"(
namespace serde {
// Serialize specialization
template<typename T>
struct Serialize<T, std::enable_if_t<std::is_same_v<T, )" << e.name() << R"(>>> {
  static void serialize(Serializer& ser, const T& val) {
    ser.serialize_struct_begin();
)";
            // serialize member variables
            for (auto& member : class_)
            {
                if (member.kind() == cppast::cpp_entity_kind::member_variable_t) {
                  const auto& member_var = static_cast<const cppast::cpp_member_variable&>(member);
                  outfile << "      ser.serialize_struct_field(\""
                          << member_var.name() << "\", val."
                          << member_var.name() << ");\n";
                }
            }
      outfile <<
R"(    ser.serialize_struct_end();
  }
};
// Deserialize specialization
template<typename T>
struct Deserialize<T, std::enable_if_t<std::is_same_v<T, )" << e.name() << R"(>>> {
  static void deserialize(Deserializer& de, T& val) {
    de.deserialize_struct_begin();
)";
            // deserialize member variables
            for (auto& member : class_)
            {
                if (member.kind() == cppast::cpp_entity_kind::member_variable_t) {
                  const auto& member_var = static_cast<const cppast::cpp_member_variable&>(member);
                  outfile << "      de.deserialize_struct_field(\""
                          << member_var.name() << "\", val."
                          << member_var.name() << ");\n";
                }
            }
      outfile <<
R"(    de.deserialize_struct_end();
  }
};
} // namespace serde

)";
            }
        });

}

int main(int argc, char* argv[])
{
  cxxopts::Options option_list(
      "serde_attr",
      "serde_attr - Serde attribute parser and serialization generator.\n");

  option_list.add_options()
    ("h,help", "display this help and exit")
    ("version", "display version information and exit")
    ("v,verbose", "be verbose when parsing")
    ("fatal_errors", "abort program when a parser error occurs, instead of doing error correction");

  option_list.add_options("compilation")
    ("infile", "the file that are being parsed",
     cxxopts::value<std::string>())
    ("outfile", "the output file that will be generated",
     cxxopts::value<std::string>())
    ("database_dir", "set the directory where a 'compile_commands.json' file is located containing build information",
    cxxopts::value<std::string>())
    ("database_file", "set the file name whose configuration will be used regardless of the current file name",
    cxxopts::value<std::string>())
    ("I,include_directory", "add directory to include search path",
     cxxopts::value<std::vector<std::string>>());

  auto options = option_list.parse(argc, argv);
  if (options.count("help")) {
    std::cout << option_list.help() << std::endl;
  }
  else if (options.count("version")) {
    std::cout << "serde_attr version 0.1.0\n";
    std::cout << "Copyright (C) Natanael J. Rabello <natanaeljrabello@gmail.com>\n";
    std::cout << '\n';
    std::cout << "Using libclang version " << CPPAST_CLANG_VERSION_STRING << '\n';
  }
  else if (!options.count("infile") || options["infile"].as<std::string>().empty()) {
    std::cerr << "missing infile argument" << '\n';
    return 1;
  }
  else if (!options.count("outfile") || options["outfile"].as<std::string>().empty()) {
    std::cerr << "missing outfile argument" << '\n';
    return 1;
  }
  else {
        // the compile config stores compilation flags
        cppast::libclang_compile_config config;
        if (options.count("database_dir"))
        {
            cppast::libclang_compilation_database database(
                options["database_dir"].as<std::string>());
            if (options.count("database_file"))
                config
                    = cppast::libclang_compile_config(database,
                                                      options["database_file"].as<std::string>());
            //else
                //config
                    //= cppast::libclang_compile_config(database, options["file"].as<std::string>());
        }

    //cppast::libclang_compile_config config;
    config.fast_preprocessing(1);
    cppast::compile_flags flags;
    config.set_flags(cppast::cpp_standard::cpp_17, flags);
    if (options.count("include_directory"))
      for (auto& include : options["include_directory"].as<std::vector<std::string>>())
        config.add_include_dir(include);

    cppast::stderr_diagnostic_logger logger;
    if (options.count("verbose"))
      logger.set_verbose(true);

    const auto& infilename = options["infile"].as<std::string>();

    std::string outfilename = options["outfile"].as<std::string>();
    auto basepath = std::filesystem::path(outfilename).remove_filename();
    std::filesystem::create_directories(basepath);

    // pre-create output file for parsing #include of generated file
    std::ofstream outfile(outfilename);
    outfile.close();

    auto file = parse_file(config, logger, infilename, options.count("fatal_errors") == 1);
    if (!file) return 2;

    print_ast(std::cout, *file);

    outfile.open(outfilename);
    generate_serde(outfile, *file);
  }

  return 0;
}
