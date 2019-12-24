#include "hddldemo.h"
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <QApplication>

static int g_rows = 2;
static int g_cols = 4;
static const char *g_launch_file = nullptr;

static void print_usage (const char* program_name, int exit_code)
{
  printf ("Usage: %s...\n", program_name);
  printf (
      " -r --row number in display window.\n"
      " -c --column number in display window.\n"
      " -l --launch file to create pipeline.\n"
      "-h --help Display this usage information.\n");
  exit (exit_code);
}


static bool parse_cmdline (int argc, char *argv[])
{
  const char* const brief = "hr:c:l:";
  const struct option details[] = {
	  {"row", 1, nullptr, 'r', },
          {"col", 1, nullptr, 'c', },
          {"col", 1, nullptr, 'l', },
          {"help", 0, nullptr, 'h',},
          {nullptr, 0, nullptr, 0}};

  int opt = 0;
  while (opt != -1) {
      opt = getopt_long(argc, argv, brief, details, nullptr);
      switch (opt) {
      case 'r':
        g_rows  = atoi(optarg);
        break;
      case 'c':
        g_cols = atoi(optarg);
        break;
      case 'l':
        g_launch_file = optarg;
        break;
      case 'h': /* help */
        print_usage (argv[0], 0);
        break;
      case '?': /* an invalid option. */
        print_usage (argv[0], 1);
        break;
      case -1: /* Done with options. */
        break;
      default: /* unexpected. */
        print_usage (argv[0], 1);
        abort ();
    }
  }
  return true;
}

void read_file(const char *file_path, std::string &desc) {
  std::string line;
  std::ifstream file(file_path);
  if (file.is_open()) {
    while (getline(file, line)) {
      desc.append(line);
    }
    file.close();
  } else {
    std::cout << "Unable to open file " << file_path << std::endl;
    exit(1);
  }
}



int main(int argc, char *argv[])
{

  parse_cmdline(argc, argv);
  if (g_launch_file == nullptr || g_rows <= 0 || g_cols <= 0) {
      std::cout << "Invalid parameter" << std::endl;
      return -1;
  }
  std::string pipelineStr;
  read_file(g_launch_file, pipelineStr);

  QString pipeline=QString::fromStdString(pipelineStr);

  QApplication a(argc, argv);
  HDDLDemo w(pipeline, g_rows, g_cols);
  //w.show();
  w.showFullScreen();

  return a.exec();
}
