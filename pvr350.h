#ifndef _PVR350_H_
#define _PVR350_H_


class cPluginPvr350 : public cPlugin {
private:
  // Add any member variables or functions you may need here.
  cPvr350Device *pvr350device;
public:
  cPluginPvr350(void);
  virtual ~cPluginPvr350();
  virtual const char *Version(void);
  virtual const char *Description(void);
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Initialize(void);
  virtual bool Start(void);
  virtual void Housekeeping(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
  virtual cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode);
  virtual const char **SVDRPHelpPages(void);
  };
#endif
