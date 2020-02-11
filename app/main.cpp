#include "hddldemo.h"
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <QApplication>
#include "ConfigParser.h"

static int g_rows = 2;
static int g_cols = 4;
static const char *g_launch_file = nullptr;

static void print_usage (const char* program_name, int exit_code)
{
  printf ("Usage: %s...\n", program_name);
  printf (
      " -c --config file to create pipeline.\n"
      "-h --help Display this usage information.\n");
  exit (exit_code);
}


static bool parse_cmdline (int argc, char *argv[])
{
  const char* const brief = "hc:";
  const struct option details[] = {
	  {"config", 1, nullptr, 'c', },
          {"help", 0, nullptr, 'h',},
          {nullptr, 0, nullptr, 0}};

  int opt = 0;
  while (opt != -1) {
      opt = getopt_long(argc, argv, brief, details, nullptr);
      switch (opt) {
      case 'c':
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


int main(int argc, char *argv[])
{

  parse_cmdline(argc, argv);
  
  if(!ConfigParser::instance()->loadConfigFile(g_launch_file)){
      return EXIT_FAILURE;
  }

  QApplication a(argc, argv);
  HDDLDemo w;
  //w.show();
  w.showFullScreen();

  return a.exec();
}
