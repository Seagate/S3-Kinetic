__author__ = 'igomez'
import enchant
import os
import re
us_dict = enchant.Dict("en_US")
ERRORS = 0


def read_string(string, line_number, filename):
    global ERRORS
    sub_string = re.search('("){1}([\w\s\D]+)("){1}', string)
    string_check = ''
    quotation_counter = 0
    single_quote_flag = False
    if sub_string:
        for c in sub_string.group(0):
            if c == '"' or c == " ":
                if quotation_counter > 0:
                    if string_check != "":
                        if not us_dict.check(string_check):
                            print "Spelling error", "'"+string_check+"'", filename, line_number
                            ERRORS += 1
                        string_check = ""
                    if c == '"':
                        quotation_counter = 0
                elif c == '"':
                    quotation_counter += 1
            else:
                if re.match('[\w]', c) and quotation_counter > 0:
                    if single_quote_flag:
                        string_check = string_check + "'" + c
                        single_quote_flag = False
                    else:
                        string_check += c
                elif c == "'":
                    single_quote_flag = True
                else:
                    if single_quote_flag:
                        single_quote_flag = False


def read_file(filename):
    f = open(filename, 'r')
    content = f.readlines()
    log_flag = False
    line_number = 1
    for line in content:
        if re.search("(//NO_SPELL)", line):
            pass
        elif re.search(".LOG\(", line):
            if not re.search(";", line):
                log_flag = True
            read_string(line, line_number, filename)

        elif log_flag:
            if (re.search(";", line)):
                log_flag = False
                read_string(line, line_number, filename)

            else:
                read_string(line, line_number, filename)
        line_number += 1

if __name__ == "__main__":
    os.chdir('./src')
    for filename in os.listdir('.'):
        read_file(filename)
        print "Done Processing ", filename
    print "There are " + str(ERRORS) + " misspellings"
