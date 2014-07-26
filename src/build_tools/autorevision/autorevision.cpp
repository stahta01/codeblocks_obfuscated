/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "tinyxml/tinyxml.h"

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <cstring>

#ifdef __WIN32__
    #define WIN32_LEAN_AND_MEAN 1
    #define NOGDI
    #include <windows.h>
    inline void set_env(const char* k, const char* v) { SetEnvironmentVariable(k, v); }
#else
    #include <cstdlib>
    inline void set_env(const char* k, const char* v) { setenv(k, v, 1); }
#endif

// Following two macros may be already defined as a check for errors in system headers...
// If they are not defined, define them in non-harming way so they "report" success.
#if !defined WIFEXITED
    #define WIFEXITED(x) 1
#endif

#if !defined WEXITSTATUS
    #define WEXITSTATUS(x) x
#endif

bool QuerySvn(const std::string& workingDir, std::string& revision, std::string& date);
bool QuerySvnOldStyle(const std::string& workingDir, std::string& revision, std::string& date );
bool QueryGitSvn(const std::string& workingDir, std::string& revision, std::string& date);
bool WriteOutput(const std::string& outputFile, std::string& revision, std::string& date);
bool SendQueryViaPipeStream(const std::string& in_Query, std::string& out_Response);

bool CheckSvnExists();
bool CheckGitExists();

bool do_int       = false;
bool do_std       = false;
bool do_wx        = false;
bool do_translate = false;
bool be_verbose   = false;
bool is_debugged  = false;
bool skip_git_svn = false;

void showHelpScreen()
{
    std::string executable;
#if WX_VERSION>30
    executable = "autorevision30";
#else
    executable = "autorevision";
#endif
               /* Screen width                                                                      */
               /*          10        20        30        40        50        60        70        80 */
               /* 123456789A123456789B123456789C123456789D123456789E123456789F123456789G123456789H  */
    std::cout << "Usage: " << executable << "[options] directory [autorevision.h]" << '\n';
    std::cout << "Options:" << '\n';
    std::cout << "    -v,   --verbose        be verbose" << '\n';
    std::cout << "          --debug          so you want even more information?" << '\n';
    std::cout << "    -h,   --help           display help (this screen) and exit" << '\n';
    std::cout << '\n';
    std::cout << "    +int                   assign const unsigned int" << '\n';
    std::cout << "    +std                   assign const std::string" << '\n';
    std::cout << "    +wx                    assing const wxString" << '\n';
    std::cout << "    +t                     add Unicode translation macros to strings" << '\n';
    std::cout << '\n';
    std::cout << "    --skip-git-svn         do not query git-svn if svn fails" << '\n';
    std::cout << '\n';
    std::cout << "    --revision [number]    set custom revision number" << '\n';
    std::cout << std::endl;
}


int main(int argc, char** argv)
{
    // ensure we have an english environment, needed
    // to correctly parse output of localized (git) svn info
#ifndef __MINGW32__
    setenv("LC_ALL", "C", 1);
#else
    setlocale(LC_ALL, "C");
#endif
    
    std::string outputFile;
    std::string workingDir;
    
    std::string overrideRevisionNumber;
    
    for(int i = 1; i < argc; ++i)
    {
        if( is_debugged )
            std::cout << "command line option " << i << ": " << argv[i] << std::endl;
        
        if(strcmp("+int", argv[i]) == 0)
            do_int = true;
        else if(strcmp("+std", argv[i]) == 0)
            do_std = true;
        else if(strcmp("+wx", argv[i]) == 0)
            do_wx = true;
        else if(strcmp("+t", argv[i]) == 0)
            do_translate = true;
        else if( strcmp("-v", argv[i]) == 0
              || strcmp("--verbose", argv[i]) == 0 )
            be_verbose = true;
        else if( strcmp("--debug", argv[i]) == 0 )
            is_debugged = true;
        else if( strcmp("-h", argv[i]) == 0
              || strcmp("--help", argv[i]) == 0 )
        {
            showHelpScreen();
            return 1;
        }
        else if(strcmp("--skip-git-svn", argv[i]) == 0)
            skip_git_svn = true;
        else if(strcmp("--revision", argv[i]) == 0)
        {
            overrideRevisionNumber.assign(argv[++i]);
        }
        // Check against possible user errors like +v instead of -v
        // to prevent creating a file with this name and errors
        else if( argv[i][0] == '+'
              || argv[i][0] == '-'
              || argv[i][0] == '/'
              || argv[i][0] == '\\' )
        {
            std::cout << "Warning: Unknown command line option " << argv[i]
                      << ". Didn't you misspell it?" << '\n'
                      << "Use --help to see available options." << std::endl;
        }
        else if(workingDir.empty())
            workingDir.assign(argv[i]);
        else if(outputFile.empty())
            outputFile.assign(argv[i]);
        else
        {
            std::cout << "Warning: Ignoring unknown command line option " << argv[i] << "." << std::endl;
        }
    }
    
    if (workingDir.empty())
    {
        showHelpScreen();
        return 1;
    }
    
    if(outputFile.empty())
        outputFile.assign("autorevision.h");
    
    if( is_debugged )
        be_verbose = true;
    
    if( is_debugged )
    {
        std::cout << "You may notice I'm little bit more verbose than usual - hey You asked for it." << std::endl;
    
        std::cout << "I should output Revision number (and date) as:\n"
                    << "        const unsigned int    " << ( do_int ? "yes" : "no" ) << '\n'
                    << "        std::string           " << ( do_std ? "yes" : "no" ) << '\n'
                    << "        wxString              " << ( do_wx  ? "yes" : "no" ) << '\n'
                  << "Will use Unicode translation macros for strings:\n"
                    << "                              " << ( do_translate ? "yes" : "no" ) << '\n'
                  << "Should I skip git-svn?\n"
                    << "                              " << ( skip_git_svn ? "yes" : "no" ) << '\n'
                  << std::endl;
        
        bool do_override = !overrideRevisionNumber.empty();
        std::cout << "Do You want to override revision number?\n"
                    << "                              "
                                                << ( do_override ? "yes" : "no" ) << '\n';
        if( do_override )
            std::cout << "             Revision number: " << overrideRevisionNumber << '\n';
        std::cout << std::endl;
        
        std::cout << "Working directory:    " << workingDir << '\n'
                  << "Output file:          " << outputFile << '\n'
                  << std::endl;
    }
    
    std::string revision = "0";
    std::string date = "unknown date";
    
    bool success = false;
    bool svnExists = CheckSvnExists();
    bool gitExists = CheckGitExists();
    
    if( !svnExists )
        std::cout << "Warning: Svn not found, skipping querying svn..." << std::endl;
    if( !gitExists )
        std::cout << "Warning: Git not found, skipping querying git..." << std::endl;
    
    if( svnExists )
    {
        success = QuerySvn( workingDir, revision, date );
        
        // Try old style, if not successful
        if( !success )
            success = QuerySvnOldStyle( workingDir, revision, date );
    }
    
    if( !success && gitExists && !skip_git_svn )
    {
        success = QueryGitSvn(workingDir, revision, date);
    }
    
    if( !success )
        std::cout << "Warning: Could not get revision info from svn or git-svn." << std::endl;
    
    if( !overrideRevisionNumber.empty() )
        revision = overrideRevisionNumber;
    
    if( !do_int && !do_std && !do_wx )
    {
        std::cout << "Error: You seem to forgot to specify how do you want to output the revision "
                    << "number... Use --help for command line options." << std::endl;
        return EXIT_FAILURE;
    }
    
    success = WriteOutput(outputFile, revision, date);
    
    if( !success )
    {
        std::cout << "Error: Could not output revision number to the header file... "
                    << "If you depend on this file, your build will probably fail. Sorry.\n"
                    << "Try adding -v or --debug to command line options to get verbose output."
                    << std::endl;
        return EXIT_FAILURE;
    }
    
    if( be_verbose )
        std::cout << "Finished..." << std::endl;
    
    return EXIT_SUCCESS;
}



bool SendQueryViaPipeStream( const std::string& in_query, std::string& out_response )
{
    if( is_debugged )
        std::cout << '\n' << "Sending query: \"" << in_query << "\"" << std::endl;
    
    FILE* fp = popen( in_query.c_str(), "r" );
    
    if(fp)
    {
        char buf[16384] = {'\0'};
        size_t charRead = fread( buf, sizeof buf[0], 16384, fp );
        if( ferror(fp) )
        {
            pclose(fp);
            
            std::cout << '\n' << "Error occured while reading from pipe stream." << std::endl;
            perror("Error");
            
            out_response = "";
            return false;
        }
        
        int ret = pclose(fp);
        
        int ifExited = WIFEXITED(ret);
        int exitStatus = WEXITSTATUS(ret);
        out_response = std::string( buf, charRead );
        
        if( is_debugged )
            std::cout << "Got response:\n[" << out_response << "]" << std::endl
                << "WIFEXITED: " << ifExited << std::endl
                << "WEXITSTATUS: " << exitStatus << std::endl;
        
        if( !ifExited || (exitStatus != 0) )
        {
            // As a hint, tell the user that the application/command/etc.
            // she's trying to call probably doesn't exist in the system
            if( be_verbose && (exitStatus == 127) )
            {
                std::string::size_type pos = in_query.find(' ');
                if( pos != in_query.npos )
                {
                    std::cout << in_query.substr( 0, pos ) << " is probably not installed."
                              << std::endl;
                }
            }
            
            return false;
        }
        else
            return true;
    }
    else
    {
        std::cout << '\n' << "-> Cannot successfuly open pipe stream to another process."
                  << std::endl;
        perror("Error");
        
        out_response = "";
        return false;
    }
}



bool CheckSvnExists()
{
    std::string cmd = "svn --version 2>&1";
    std::string response;
    
    if( be_verbose )
        std::cout << "Checking if svn exists... ";
    
    if( !SendQueryViaPipeStream( cmd, response ) )
    {
        if( be_verbose )
            std::cout << "Not found" << std::endl;
        
        return false;
    }
    else
    {
        if( be_verbose )
            std::cout << "Found" << std::endl;
        
        return true;
    }
}



bool CheckGitExists()
{
    std::string cmd = "git --version 2>&1";
    std::string response;
    
    if( be_verbose )
        std::cout << "Checking if git exists... ";
    
    if( !SendQueryViaPipeStream( cmd, response ) )
    {
        if( be_verbose )
            std::cout << "Not found" << std::endl;
        
        return false;
    }
    else
    {
        if( be_verbose )
            std::cout << "Found" << std::endl;
        
        return true;
    }
}



bool QueryGitSvn(const std::string& workingDir, std::string& revision, std::string &date)
{
    std::string svncmd = "git svn info ";
    svncmd.append(workingDir);
    svncmd.append(" 2>&1");
    
    if( be_verbose )
        std::cout << "Querying git-svn for revision info... ";
    if( is_debugged )
        std::cout << '\n' << "(" << svncmd << ")" << std::endl;
    
    FILE* svn = popen(svncmd.c_str(), "r");
    // second try git svn info
    if(svn)
    {
        char buf[16384] = {'\0'};
        size_t char_count = fread(buf, sizeof buf[0], 16384, svn);
        bool reached_eof = feof(svn);
        int ret = pclose(svn);
        
        if( !reached_eof )
        {
            if( be_verbose )
                std::cout << "Unsuccessful" << std::endl;
            
            std::cout << "Error: did not reach end of file while reading information." << std::endl;
            return false;
        }
        
        std::string output( buf, char_count );
        
        if( is_debugged )
            std::cout << "Got response:\n[" << output << "]" << std::endl
                << "WIFEXITED: " << WIFEXITED(ret) << std::endl
                << "WEXITSTATUS: " << WEXITSTATUS(ret) << std::endl;
        
        if (!WIFEXITED(ret) || (WEXITSTATUS(ret) != 0))
        {
            if( be_verbose )
                std::cout << "Unsuccessful" << std::endl;
            if( is_debugged )
                std::cout << "-> Git didn't exit successfuly." << std::endl;
            return false;
        }
        
        std::string what("Last Changed Rev: ");
        std::string::size_type pos = std::string::npos;
        std::string::size_type len = 0;
        pos = output.find(what);
        if (pos != std::string::npos)
        {
            pos += what.length();
            len = 0;
            // revision must be numeric
            while (buf[ pos + len ] >= '0' && buf[ pos + len++ ] <= '9')
                ;
        }
        if (len != 0)
        {
            revision = output.substr(pos, len);
        }
        else
        {
            if( be_verbose )
                std::cout << "Unsuccessful" << std::endl;
            
            std::cout << "Error: Cannot parse revision number from git svn response." << std::endl;
            return false;
        }
        
        what = "Last Changed Date: ";
        pos = output.find(what);
        if (pos != std::string::npos)
        {
            pos += what.length();
            len = output.find(" ", pos);
            // we want the position of the second space
            if (len != std::string::npos)
                len = output.find(" ", len + 1);
            if (len != std::string::npos)
                len -= pos;
            else
                len = 0;
        }
        if (len != 0)
        {
            date = output.substr(pos, len);
        }
        else
        {
            if( be_verbose )
                std::cout << "Unsuccessful" << std::endl;
            
            std::cout << "Error: Cannot parse date format from git svn response." << std::endl;
            return false;
        }
        
        if( be_verbose )
        {
            std::cout << "Success" << '\n'
                      << "    Found revision: " << revision << std::endl
                      << "    Found date:     " << date << '\n';
        }
        return true;
    }
    else
    {
        if( be_verbose )
            std::cout << "Unsuccessful" << std::endl;
        if( is_debugged )
            std::cout << "-> Cannot successfuly open pipe stream to another process." << std::endl;
        return false;
    }
    
    // if we reached here, we couldn't get the info
    if( be_verbose )
        std::cout << "Unsuccessful..." << std::endl;
    return false;
}



bool QuerySvnOldStyle(const std::string& workingDir, std::string& revision, std::string& date)
{
    // third try oldstyle (outated) svn info (should not be needed anymore)
    std::string svncmd = "svn info --non-interactive ";
    svncmd.append(workingDir);
    svncmd.append(" 2>&1");
    
    if( be_verbose )
        std::cout << "Querying svn the old style... ";
    if( is_debugged )
        std::cout << '\n' << "(" << svncmd << ")" << std::endl;
    
    FILE* svn = popen(svncmd.c_str(), "r");
    
    if(svn)
    {
        char buf[16384] = {'\0'};
        size_t char_count = fread(buf, sizeof buf[0], 16384, svn);
        bool reached_eof = feof(svn);
        int ret = pclose(svn);
        
        if( !reached_eof )
        {
            if( be_verbose )
                std::cout << "Unsuccessful" << std::endl;
            
            std::cout << "Error: did not reach end of file while reading information." << std::endl;
            return false;
        }
        
        std::string output( buf, char_count );
        
        if( is_debugged )
            std::cout << "Got response:\n[" << output << "]" << std::endl
                << "WIFEXITED: " << WIFEXITED(ret) << std::endl
                << "WEXITSTATUS: " << WEXITSTATUS(ret) << std::endl;
        
        if (!WIFEXITED(ret) || (WEXITSTATUS(ret) != 0))
        {
            if( be_verbose )
                std::cout << "Unsuccessful" << std::endl;
            if( is_debugged )
                std::cout << "-> Svn didn't exit successfuly." << std::endl;
            return false;
        }
        
        std::string what("Last Changed Rev: ");
        std::string::size_type pos = std::string::npos;
        std::string::size_type len = 0;
        pos = output.find(what);
        if (pos != std::string::npos)
        {
            pos += what.length();
            len = 0;
            // revision must be numeric
            while (buf[ pos + len ] >= '0' && buf[ pos + len++ ] <= '9')
                ;
        }
        if (len != 0)
        {
            revision = output.substr(pos, len);
        }
        else
        {
            if( be_verbose )
                std::cout << "Unsuccessful" << std::endl;
            
            std::cout << "Error: Cannot parse revision number from svn old-style response." << std::endl;
            return false;
        }
        
        what = "Last Changed Date: ";
        pos = output.find(what);
        if (pos != std::string::npos)
        {
            pos += what.length();
            len = output.find(" ", pos);
            // we want the position of the second space
            if (len != std::string::npos)
                len = output.find(" ", len + 1);
            if (len != std::string::npos)
                len -= pos;
            else
                len = 0;
        }
        if (len != 0)
        {
            date = output.substr(pos, len);
        }
        else
        {
            if( be_verbose )
                std::cout << "Unsuccessful" << std::endl;
            
            std::cout << "Error: Cannot parse date format from svn old-style response." << std::endl;
            return false;
        }
        
        if( be_verbose )
        {
            std::cout << "Success" << '\n'
                      << "    Found revision: " << revision << std::endl
                      << "    Found date:     " << date << '\n';
        }
        return true;
    }
    else
    {
        if( be_verbose )
            std::cout << "Unsuccessful" << std::endl;
        if( is_debugged )
            std::cout << "-> Cannot successfuly open pipe stream to another process." << std::endl;
        return false;
    }
    
    // if we are here, we could not read the info
    if( be_verbose )
        std::cout << "Unsuccessful" << std::endl;
    return false;
}



bool QuerySvn(const std::string& workingDir, std::string& revision, std::string &date)
{
    std::string svncmd("svn info --xml --non-interactive ");
    svncmd.append(workingDir);
    svncmd.append(" 2>&1");
    
    if( be_verbose )
        std::cout << "Querying svn for revision number... ";
    
    std::string output;
    if( SendQueryViaPipeStream( svncmd, output ) )
    {
        TiXmlDocument doc;
        doc.Parse( output.c_str() );
        
        if(doc.Error())
        {
            if( be_verbose )
                std::cout << "Unsuccessful" << std::endl;
            
            std::cout << "Error: Unable to parse information in XML format returned by svn."
                      << std::endl
                      << doc.ErrorDesc()
                      << std::endl;
            return false;
        }
        
        TiXmlHandle hCommit(&doc);
        hCommit = hCommit.FirstChildElement("info").FirstChildElement("entry").FirstChildElement("commit");
        if(const TiXmlElement* e = hCommit.ToElement())
        {
            revision = e->Attribute("revision") ? e->Attribute("revision") : "";
            const TiXmlElement* d = e->FirstChildElement("date");
            if(d && d->GetText())
            {
                date = d->GetText();
                std::string::size_type pos = date.find('T');
                if (pos != std::string::npos)
                {
                    date[pos] = ' ';
                }
                pos = date.rfind('.');
                if (pos != std::string::npos)
                {
                    date = date.substr(0, pos);
                }
            }
            
            if( be_verbose )
            {
                std::cout << "Success" << '\n'
                          << "    Found revision: " << revision << '\n'
                          << "    Found date:     " << date << std::endl;
            }
            
            return true;
        }
        if( be_verbose )
        {
            if( be_verbose )
                std::cout << "Unsuccessful" << std::endl;
            
            std::cout << "Error: Unable to get revision info." << std::endl;
        }
        return false;
    }
    else
    {
        if( be_verbose )
            std::cout << "Unsuccessful" << std::endl;
        return false;
    }
}



bool WriteOutput(const std::string& outputFile, std::string& revision, std::string& date)
{
    
    // Create a string we can quickly use to compare if we already
    //  have all the required information in the header file since
    //  last time. Generate new header file only if requirements
    //  or revision number changes.
    // - We do this check to save compilation time, since if we
    //  rewrite the header even with the exact text, compiler will
    //  still think it has changed and it will require recompilation.
    std::stringstream ssWouldBeCurrentVersionTag;
    ssWouldBeCurrentVersionTag << "/* " 
                                << "revision:" << revision << ";"
                                << "date:" << date << ";" // is revision date, not file date
                                << std::boolalpha
                                << "do_int:" << do_int << ";"
                                << "do_std:" << do_std << ";"
                                << "do_translate:" << do_translate << ";"
                                << "do_wx:" << do_wx
                                << " */";
    {
        std::string oldVersionTag;
        
        std::ifstream in(outputFile);
        if( in.is_open() )
        {
            std::getline( in, oldVersionTag );
            
            if( oldVersionTag.compare( ssWouldBeCurrentVersionTag.str() ) == 0 )
            {
                if(be_verbose)
                    std::cout << "Revision unchanged - " << revision << ". Nothing to do here..." << std::endl;
                in.close();
                return true;
            }
        }
    }
    
    
    
    std::ofstream ofsHeaderFile(outputFile);
    if( !ofsHeaderFile.is_open() )
    {
        std::cout << "Error: Could not open " << outputFile << " for writing..." << std::endl;
        return false;
    }
    
    ofsHeaderFile << ssWouldBeCurrentVersionTag.str() << std::endl;
    ofsHeaderFile << "// Don't include this header, only configmanager-revision.cpp should do this.\n"
                << "#ifndef AUTOREVISION_H\n"
                << "#define AUTOREVISION_H\n"
                << "\n"
                << std::endl;
    
    if( do_std )
        ofsHeaderFile << "#include <string>\n";
    if( do_wx )
        ofsHeaderFile << "#include <wx/string.h>\n";
    
    if( do_int || do_std || do_wx )
        ofsHeaderFile << "\n"
            << "namespace autorevision\n"
            << "{\n";
    
    if( do_int )
        ofsHeaderFile << "\tconst unsigned int svn_revision = " << revision << ";\n";
    
    if( do_translate )
    {
        revision = "_T(\"" + revision + "\")";
        date = "_T(\"" + date + "\")";
    }
    else
    {
        revision = "\"" + revision + "\"";
        date = "\"" + date + "\"";
    }
    
    if( do_std )
        ofsHeaderFile << "\t const std::string svn_revision_s(" << revision << ");\n";
    if( do_wx )
        ofsHeaderFile << "\tconst wxString svnRevision(" << revision << ");\n";
    
    if( do_std )
        ofsHeaderFile << "\tconst std::string svn_date_s(" << revision << ");\n";
    if( do_wx )
        ofsHeaderFile << "\tconst wxString svnDate(" << date << ");\n";
    
    if( do_int || do_std || do_wx )
        ofsHeaderFile << "}\n\n";
    
    ofsHeaderFile << "\n\n" << "#endif // AUTOREVISION_H\n";
    
    ofsHeaderFile.close();
    
    if( be_verbose )
        std::cout << "Done" << std::endl;
    
    return true;
}

