#ifdef WIN32
#include "getopt.h"
#else
#include <unistd.h>
#endif

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sstream>
#include <iostream>
#include <fstream>
#include <cstdlib> // rand, srand

#include <map>
#include <string>
#include <vector>

#include "func.h"

//#include <gsl/gsl_linalg.h>
//#include <gsl/gsl_rng.h>

using namespace std;
//using namespace boost;

static const char * help=
        "\
        Usage: postprocess [OPTIONS] \n\
            \n\
            Options:\n\
                -i file.gz \n\
                -d dir_each_ordering \n\
                -l strainHapOrderFile \n\
                -o strainFineOrderFile \n\
                -r dir_results \n\
                -t 1|2 \n\
                -p 010|011|020|030 \n\
                -s .01 (suffix added to the the output file) \n\
                [-m pos2missingInd.txt] \n\
                [-c donor_recipient_constraint.txt] \n\
                \n";


// ######################################################################
// global variables
// ######################################################################

//gsl_rng * r;  /* global generator */
const int N_BOOTSTRAP = 100;

const int MAX_BUFFER = 10240; 

const char * LOOP_010 = "010";
const char * LOOP_011 = "011";
const char * LOOP_020 = "020";
const char * LOOP_030 = "030";

// part1 - output and input
const char * out_each_dir_sum                  = "sum.txt"; // in each ordering dir
const char * out_each_dir_averave_matrix       = "average.matrix.txt"; // in each ordering dir

// part 2 - output
const char * out_each_dir_site_distScore_info  = "site_distScore.txt"; // in each ordering dir

// part 3 - one of the inputs and ouptut
const char * results_summary_pos                    = "results_siteStats_summary.pos.txt"; // in the results dir
const char * out_each_dir_site_minus_average_matrix = "site_minus_average.matrix.summary"; // in each ordering dir


// ######################################################################
// util
// ######################################################################
FILE * fopen_wrapper(const char * filename, const char * mode);
int getRandom(int min,int max);

FILE * fopen_wrapper(const char * filename, const char * mode) {

    FILE * f = fopen(filename, mode);

    if (f == NULL) {
        printf("Failed to open file %s\n", filename);
    }
    return f;
}

int getRandom(int min,int max)
{
    return min + (int)(rand()*(max-min+1.0)/(1.0+RAND_MAX));
}


// ######################################################################
// main 
// ######################################################################
int main(int argc, char **argv)
{

    // getopt
    int c;
    bool verbose=false;
    char * inFileName=NULL;
    char * dir_each_ordering=NULL;
    char * strainHapOrderFile=NULL;
    char * dir_results=NULL;
    char * strainFineOrderFile=NULL;
    char * suffix=NULL;
    int type_painting=-1;
    char * loop_part=NULL;
    int seed=-1; // constant
    char * pos2missingIndFile=NULL;
    char * donor_recipient_constraintFile=NULL;

    if (argc==1) {printf("%s",help);exit(0);}
    while ((c = getopt (argc, argv, "i:d:l:r:o:t:p:s:m:c:v")) != -1)
        switch (c)
    {
        case('i'):inFileName=optarg;break;
        case('d'):dir_each_ordering=optarg;break;
        case('l'):strainHapOrderFile=optarg;break;
        case('o'):strainFineOrderFile=optarg;break;
        case('r'):dir_results=optarg;break; // used only in loop_part is 030
        case('t'):type_painting=atoi(optarg);break;
        case('p'):loop_part=optarg;break;
        case('s'):suffix=optarg;break;
        case('m'):pos2missingIndFile=optarg;break;
        case('c'):donor_recipient_constraintFile=optarg;break;
        case('v'):verbose=true;break;
        case '?':
            if (isprint (optopt))
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf (stderr,
                "Unknown option character `\\x%x'.\n",
                optopt);
            return 1;
        default:
            abort ();
    }

    //printf("%s\n",dir_ordering_listFile);
    //cout << string(dir_ordering_listFile) << endl;

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // variables
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    int i;
    int j;

    //
    // record of painted individuals in each ordering
    //
    int cnt_recipient; // 1-indexed, 
    int cnt_donor; // 1-indexed
    //
    // cnt_recipient, cnt_donor => pair<int, int> 
    //
    string recipient_name = "";
    string donor_name = "";
    map<string, int> hash_strainName2IND; // 1-indexed
    map<int, string> hash_strainIND2Name; // 1-indexed
    vector<string> arr_indName_eachOrdering; 


    FILE *fh;
    FILE *fh_out;
    char fname[512];
    char buffer[MAX_BUFFER];
    char buffer2[MAX_BUFFER];
    char** arr_line = (char**)calloc(10000 , sizeof(char*));

    FILE *fh_inc_exc;
    char fname_verbose[512];
    int pos_i_included = 0;
    int pos_i_excluded = 0;
    double pos_sum_distScore_included = 0;
    double pos_sum_distScore_excluded = 0;

    int num_site=0;
    double donorInfoContent;
    double rowsum_prob;
    double distScore_per_mat;
    string out_header;

    stringstream ss;
    time_t timer;
    char *stamp;
    bool exclude_ind_flag;
    bool skip_calc_flag;

    vector<string> arr_indName_outDispOrdering; 

    map<pair<int, int>, double> hash_summation; 
    map<pair<int, int>, double> hash_average_prob; 
    //map<pair<int, int>, string> hash_average_probStr; // double or NA
    map<pair<int, int>, double> hash_site_by_site_prob; 

    map<int, string> hash_summaryPos2Type;
    map<pair<int, int>, double> hash_site_minus_ave;

    map<pair<int, string>, int> hash_pos_missing_ind;

    string donor_or_recipient = "";
    string strainName = "";
    int strainIND = -1; // 1-indexed
    map<string, int> hash_constrained_donors; 
    map<string, int> hash_constrained_recipients; 


    // ########################################################################################
    srand ( seed );

    //
    // read pos2missingIndFile (if specified)
    //
    if (pos2missingIndFile != NULL && pos2missingIndFile[0] != '\0') {
        fh = fopen_wrapper(pos2missingIndFile, "r");
        while (!feof(fh)) {
            if (fgets(buffer, MAX_BUFFER, fh) != NULL) {
                buffer[strlen(buffer) - 1] =  '\0';

                int pos;
                *arr_line = strtok(buffer , "\t");
                pos = atoi(*arr_line);

                for (i = 1; ; i++) { 
                    if (NULL == (*(arr_line+i) = strtok(NULL , "\t"))){
                        break;
                    }
                    if (i == 1) {
                        donor_name = string(*(arr_line+i)); 
                        //recipient_name = string(*(arr_line+i)); 
                        break;
                    }
                }

                hash_pos_missing_ind[pair<int,string>(pos,donor_name)] = 1;
                //hash_pos_missing_ind[pair<int,string>(pos,recipient_name)] = 1;
            }
        }
        fclose(fh);
    }

    //
    // read donor_recipient_constraintFile (if specified)
    //
    if (donor_recipient_constraintFile != NULL && donor_recipient_constraintFile[0] != '\0') {
        fh = fopen_wrapper(donor_recipient_constraintFile, "r");
        while (!feof(fh)) {
            if (fgets(buffer, MAX_BUFFER, fh) != NULL) {
                buffer[strlen(buffer) - 1] =  '\0';

                *arr_line = strtok(buffer , "\t");
                donor_or_recipient = string(*arr_line);

                for (i = 1; ; i++) { 
                    if (NULL == (*(arr_line+i) = strtok(NULL , "\t"))){
                        break;
                    }
                    if (i == 1) {
                        strainName = string(*(arr_line+i)); 
                        break;
                    }
                }

                if (donor_or_recipient == "donor") {
                    hash_constrained_donors[strainName] = 1;
                } else if (donor_or_recipient == "recipient") {
                    hash_constrained_recipients[strainName] = 1;
                } else {
                    printf("Info: %s will not be used because it is not either donor or recipient\n", strainName.c_str());
                }
            }
        }
        fclose(fh);
    }


    // ########################################################################################

    //
    // load arr_indName_outDispOrdering
    //
    fh = fopen_wrapper(strainFineOrderFile, "r");
    while (!feof(fh)) {
        if (fgets(buffer, MAX_BUFFER, fh) != NULL) {
            buffer[strlen(buffer) - 1] =  '\0';

            *arr_line = strtok(buffer , "\t");
            strainName = string(*arr_line);

            arr_indName_outDispOrdering.push_back(strainName);
        }
    }
    fclose(fh);

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    //
    // prepare strain name list of each ordering
    //
    fh = fopen_wrapper(strainHapOrderFile, "r");

    strainIND = 1;
    while (!feof(fh)) {
        if (fgets(buffer, MAX_BUFFER, fh) != NULL) {
            buffer[strlen(buffer) - 1] =  '\0';

            *arr_line = strtok(buffer , "\t");
            strainName = string(*arr_line);

            if (verbose) {
                //cout << strainIND << "\t" << strainName << endl;
            }
            hash_strainName2IND[strainName] = strainIND;
            hash_strainIND2Name[strainIND] = strainName;

            arr_indName_eachOrdering.push_back(strainName);

            strainIND++;
        }
    }
    fclose(fh);


    // ########################################################################################

    if ( strcmp(loop_part,LOOP_010) == 0 ) {

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
        // calculate summation
        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
        int i_row = 0;
        int i_recipient_strain_of_this_pos = 0;

        timer = time(NULL); stamp = ctime(&timer); stamp[strlen(stamp)-1] = '\0';
        printf("%s: summation of this ordering started\n", stamp);

        gzFile fileStream;
        openFile (inFileName, fileStream, "rb"); // works also if file not compressed
        string line;
        while (getline (fileStream, line)) {
            strcpy(buffer, line.c_str());
        //while(fgets(buffer, MAX_BUFFER , stdin) != NULL) {

            buffer[strlen(buffer) - 1] =  '\0'; // do it before strtok
            memcpy(buffer2, buffer, sizeof(buffer2));

            i_row++;
            if (i_row % (arr_indName_outDispOrdering.size()*1000) == 0) { // (precisely, arr_indName_outDispOrdering.size()-1)
                timer = time(NULL); stamp = ctime(&timer); stamp[strlen(stamp)-1] = '\0';
                printf("%s: i_l1=%d\n",stamp,i_row/arr_indName_outDispOrdering.size());
            }

            i_recipient_strain_of_this_pos++;

            *arr_line = strtok(buffer, " ");
            if (string(*arr_line) != "HAP" && string(*arr_line) != "pos") {

                char  *p;
                const char * tab = "\t";
                int pos;

                // get pos & cnt_recipient
                cnt_recipient = -1;
                p = strstr(*arr_line, tab);
                if (p != NULL) {
                    pos = atoi(strtok(*arr_line, tab));
                    cnt_recipient = atoi(strtok(NULL, tab));

                    if (verbose) {
                        //printf("%d\t%d\n",pos,cnt_recipient);
                    }
                } else {
                    pos = atoi(*arr_line);
                    cnt_recipient = 1;
                    for (i = 1; ; i++) { 
                        if (NULL == (*(arr_line+i) = strtok(NULL , " "))){
                            break;
                        }
                        cnt_recipient++;
                    }
                    if (verbose) {
                        //printf("%d\n",cnt_recipient);
                    }
                }

                recipient_name = hash_strainIND2Name[cnt_recipient];

                // skip the 1st column, and read all others 
                *arr_line = strtok(buffer2, " ");
                for (i = 1; ; i++) {
                    int cnt_donor = -1;
                    if (NULL == (*(arr_line+i) = strtok(NULL , " "))){
                        break;
                    }

                    if (type_painting == 2) {
                        cnt_donor = i;
                    } else {
                        if (i < cnt_recipient) {
                            cnt_donor = i;
                        } else {
                            cnt_donor = i+1;
                        }
                    }

                    //
                    // "all copy everyone else"    : scalar(@arr_line) is the same among all recipients
                    // 
                    // orderings-based conditioning: scalar(@arr_line) is variable among recipients
                    //
                    if (type_painting == 2) {
                        // conditioning
                        //   read all probabilities of donors prepared in advance
                        donor_name = hash_strainIND2Name[cnt_donor];
                    } else {
                        // "all copy everyone else" (default of chromopainter)
                        //    ncol is always n-1
                        if (cnt_donor < cnt_recipient) {
                           donor_name = hash_strainIND2Name[cnt_donor]; 
                        } else {
                           donor_name = hash_strainIND2Name[cnt_donor+1]; 
                        }
                    }

                    //
                    // for calculating the average
                    //
                    if (has_pairkey_intstring2int( hash_pos_missing_ind, pair<int,string>(pos,recipient_name) )) {
                        exclude_ind_flag = true;
                    } else if (has_pairkey_intstring2int( hash_pos_missing_ind, pair<int,string>(pos,donor_name) )) {
                        exclude_ind_flag = true;
                    } else {
                        exclude_ind_flag = false;
                    }

                    if (exclude_ind_flag == false) {
                        if ( !has_pairkey_int2double( hash_summation, pair<int,int>(cnt_recipient,cnt_donor) ) ) {
                            hash_summation[pair<int,int>(cnt_recipient,cnt_donor)] = atof(*(arr_line+i));
                        } else {
                            hash_summation[pair<int,int>(cnt_recipient,cnt_donor)] += atof(*(arr_line+i));
                        }
                    } else {
                        if ( !has_pairkey_int2double( hash_summation, pair<int,int>(cnt_recipient,cnt_donor) ) ) {
                            hash_summation[pair<int,int>(cnt_recipient,cnt_donor)] = 0;
                        }
                    }

                    if (cnt_recipient==2 && cnt_donor==1) {
                        num_site++;
                    }

                } // for column

            } // exclude headers

        } // while read line

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
        // output summation
        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
        if (suffix == NULL) {
            sprintf( fname, "%s/%s", dir_each_ordering, out_each_dir_sum ); 
        } else {
            sprintf( fname, "%s/%s%s", dir_each_ordering, out_each_dir_sum, suffix ); 
        }
        fh_out = fopen_wrapper(fname, "w");

        for (i=0; i<arr_indName_eachOrdering.size(); i++) {
            if (type_painting == 2) {
              cnt_recipient = i+1;
            } else if (type_painting == 1) {
              cnt_recipient = hash_strainName2IND[arr_indName_outDispOrdering[i]];
            }

            // diagonal is always zero
            hash_summation[pair<int,int>(cnt_recipient,cnt_recipient)] = 0;

            // output 
            for (j=0; j<arr_indName_eachOrdering.size(); j++) {
                if (type_painting == 2) {
                  cnt_donor = j+1;
                } else if (type_painting == 1) {
                  cnt_donor = hash_strainName2IND[arr_indName_outDispOrdering[j]];
                }

                if (j==0) {
                    fprintf(fh_out, "%d %.15lf", cnt_recipient, hash_summation[pair<int,int>(cnt_recipient,cnt_donor)]);
                } else {
                    fprintf(fh_out, " %.15lf", hash_summation[pair<int,int>(cnt_recipient,cnt_donor)]);
                }
            }

            fprintf(fh_out, "\n");
        }

        fprintf(fh_out, "num_site %d\n", num_site);

        timer = time(NULL); stamp = ctime(&timer); stamp[strlen(stamp)-1] = '\0';
        printf("%s: summation was written to %s\n", stamp, fname);


    } else if ( strcmp(loop_part,LOOP_011) == 0 ) {

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        // load summation to hash_average_prob
        // load num_site
        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

        // initialize
        for (int ii=1; ii<=arr_indName_eachOrdering.size(); ii++) {
            for (int jj=1; jj<=arr_indName_eachOrdering.size(); jj++) {
                if (type_painting == 2) {
                    if (ii >= jj) {
                        hash_average_prob[pair<int,int>(ii,jj)] = 0;
                    }
                } else {
                        hash_average_prob[pair<int,int>(ii,jj)] = 0;
                }
                
            }
        }

        fh = fopen_wrapper(inFileName, "r");
        while (!feof(fh)) {
            int tmp_num_site;

            if (fgets(buffer, MAX_BUFFER, fh) != NULL) {
                buffer[strlen(buffer) - 1] =  '\0';

                *arr_line = strtok(buffer , " ");
                if (string(*arr_line) == "num_site") {

                    for (i = 1; ; i++) { // always use this loop to get information
                        if (NULL == (*(arr_line+i) = strtok(NULL , " "))){
                            break;
                        }

                        if (i == 1) {
                            num_site += atoi(*(arr_line+i));
                            break;
                        }
                    }

                } else {
                    cnt_recipient = atoi(*(arr_line));

                    for (i = 1; ; i++) { 
                        if (NULL == (*(arr_line+i) = strtok(NULL , " "))){
                            break;
                        }

                        if (type_painting == 2) {
                          cnt_donor = i;
                        } else if (type_painting == 1) {
                          cnt_donor = hash_strainName2IND[arr_indName_outDispOrdering[i]];
                        }

                        if (atof(*(arr_line+i)) > 0) {
                            if ( has_pairkey_int2double( hash_average_prob, pair<int,int>(cnt_recipient,cnt_donor)) ) {
                                hash_average_prob[pair<int,int>(cnt_recipient,cnt_donor)] += atof(*(arr_line+i));
                            } else {
                                hash_average_prob[pair<int,int>(cnt_recipient,cnt_donor)] = atof(*(arr_line+i));
                            }
                        } else {
                            // do nothing 
                            // (in the ordered condition, 0 or undefined)
                        }
                    }

                }
            }
        }
        fclose(fh);

#ifdef DEBUG
        printf("num_site=%d\n", num_site);
        for (int ii=1; ii<=arr_indName_eachOrdering.size(); ii++) {
            for (int jj=1; jj<=arr_indName_eachOrdering.size(); jj++) {
                printf("%lf ",hash_average_prob[pair<int,int>(ii,jj)]);
            }
            printf("\n");
        }
#endif

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        // calculate average matrix (summation/num_site)
        //   format
        //   by using cnt_recipient, cnt_donor (of this ordering or of display ordering)
        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        sprintf( fname, "%s/%s", dir_each_ordering, out_each_dir_averave_matrix ); 
        fh_out = fopen_wrapper(fname, "w");

        out_header == "";
        for (i=0; i<arr_indName_eachOrdering.size(); i++) {
            if (type_painting == 2) {
              cnt_recipient = i+1; // 1-indexed <= 0-indexed
            } else if (type_painting == 1) {
              cnt_recipient = hash_strainName2IND[arr_indName_outDispOrdering[i]]; // <= order by outDispOrdering (i=0,...,n-1) 
            }

            if (out_header == "") {
                out_header =                    arr_indName_eachOrdering[cnt_recipient-1]; // 0-indexed
            } else {
                out_header = out_header + " " + arr_indName_eachOrdering[cnt_recipient-1];
            }
        }
        fprintf(fh_out, "%s\n", out_header.c_str());

        for (i=0; i<arr_indName_eachOrdering.size(); i++) {

            if (type_painting == 2) {
              cnt_recipient = i+1;
            } else if (type_painting == 1) {
              cnt_recipient = hash_strainName2IND[arr_indName_outDispOrdering[i]];
            }

            // diagonal is always zero
            hash_average_prob[pair<int,int>(cnt_recipient,cnt_recipient)] = 0;

            // calculate average (from sum), and rowsum_prob
            rowsum_prob = 0;
            for (j=0; j<arr_indName_eachOrdering.size(); j++) {
                if (type_painting == 2) {
                  cnt_donor = j+1;
                } else if (type_painting == 1) {
                  cnt_donor = hash_strainName2IND[arr_indName_outDispOrdering[j]];
                }

                //
                // the following "if-else" is added to implement orderings-based conditioning
                //
                if ( has_pairkey_int2double( hash_average_prob, pair<int,int>(cnt_recipient,cnt_donor) ) ) {
                    // sum => average
                    hash_average_prob[pair<int,int>(cnt_recipient,cnt_donor)] /= num_site;

                    // calculate rowsum_prob 
                    // (missing donor was set to be 0 above)
                    rowsum_prob += hash_average_prob[pair<int,int>(cnt_recipient,cnt_donor)];
                } else {
                    // do nothing
                }

            }

            // output after adjustment
            for (j=0; j<arr_indName_eachOrdering.size(); j++) {
                if (type_painting == 2) {
                  cnt_donor = j+1;
                } else if (type_painting == 1) {
                  cnt_donor = hash_strainName2IND[arr_indName_outDispOrdering[j]];
                }
                donor_name = hash_strainIND2Name[cnt_donor];

                //
                // the following "if-else" is added to implement orderings-based conditioning
                //
                if (type_painting == 2 && cnt_recipient == 1) {
                    if (cnt_donor == cnt_recipient) {
                        if (j==0) {
                            fprintf(fh_out, "0");
                        } else {
                            fprintf(fh_out, " 0");
                        }
                    } else {
                        // the first recipient is just not painted in the ordering condition
                        if (j==0) {
                            fprintf(fh_out, "-9");
                        } else {
                            fprintf(fh_out, " -9");
                        }
                    }
                } else {
                    if ( has_pairkey_int2double( hash_average_prob, pair<int,int>(cnt_recipient,cnt_donor) ) ) {
                        // adjust

                        // if there is no missing individual, rowsum_prob must be larger than 0
                        if (pos2missingIndFile == NULL) {
                            if (rowsum_prob == 0) {
                                fprintf(stderr, "Error: rowsum_prob is 0 (hash_average_prob) for recipient=%d, donor=%d\n", 
                                    cnt_recipient, cnt_donor);
                                exit(1);
                            } else {
                                hash_average_prob[pair<int,int>(cnt_recipient,cnt_donor)] /= rowsum_prob;
                            }
                        // if there is missing individual
                        } else {
                            if (rowsum_prob == 0) {
                                hash_average_prob[pair<int,int>(cnt_recipient,cnt_donor)] = -9;
                            } else {
                                hash_average_prob[pair<int,int>(cnt_recipient,cnt_donor)] /= rowsum_prob;
                            }
                        }

                        // output
                        if (j==0) {
                            fprintf(fh_out, "%.15lf", hash_average_prob[pair<int,int>(cnt_recipient,cnt_donor)]);
                        } else {
                            fprintf(fh_out, " %.15lf", hash_average_prob[pair<int,int>(cnt_recipient,cnt_donor)]);
                        }
                    } else {
                        if (j==0) {
                            fprintf(fh_out, "-9");
                        } else {
                            fprintf(fh_out, " -9");
                        }
                    }
                }
            } // donor loop

            fprintf(fh_out, "\n");

        } // recipient loop


    } else {

        if (verbose) {
            sprintf( fname_verbose, "%s/verbose_inc_exc.txt", dir_each_ordering ); 
            fh_inc_exc = fopen_wrapper(fname_verbose, "w");
        }

        //
        // restore average matrix (common in loop 2 and 3)
        //
        sprintf( fname, "%s/%s", dir_each_ordering, out_each_dir_averave_matrix ); 

        fh = fopen_wrapper(fname, "r");

        // skip header
        fgets(buffer, MAX_BUFFER, fh);

        i = 0;
        while (!feof(fh)) {
            if (fgets(buffer, MAX_BUFFER, fh) != NULL) {
                buffer[strlen(buffer) - 1] =  '\0';
                *arr_line = strtok(buffer , " ");

                if (type_painting == 2) {
                    cnt_recipient = i+1;
                } else if (type_painting == 1) {
                    cnt_recipient = hash_strainName2IND[arr_indName_outDispOrdering[i]];
                }

                j = 0;
                if (type_painting == 2) {
                  cnt_donor = j+1;
                } else if (type_painting == 1) {
                  cnt_donor = hash_strainName2IND[arr_indName_outDispOrdering[j]];
                }
                hash_average_prob[pair<int,int>(cnt_recipient, cnt_donor)] = atof(*arr_line);

                for (j = 1; ; j++) { 
                    if (NULL == (*(arr_line+j) = strtok(NULL , " "))){
                        break;
                    }

                    if (type_painting == 2) {
                      cnt_donor = j+1;
                    } else if (type_painting == 1) {
                      cnt_donor = hash_strainName2IND[arr_indName_outDispOrdering[j]];
                    }
                    hash_average_prob[pair<int,int>(cnt_recipient, cnt_donor)] = atof(*(arr_line+j));
                }

                i++;
            }
        }

        fclose(fh);


        //
        // loop 2 or 3, separately
        //
        if (strcmp(loop_part,LOOP_020) == 0) {

            // ************************************************************************
            // calculate the distance statistic for each site
            // ************************************************************************
            if (suffix == NULL) {
                sprintf( fname, "%s/%s", dir_each_ordering, out_each_dir_site_distScore_info ); 
            } else {
                sprintf( fname, "%s/%s%s", dir_each_ordering, out_each_dir_site_distScore_info, suffix ); 
            }
            fh_out = fopen_wrapper(fname, "w");

            int i_row = 0;
            int i_recipient_strain_of_this_pos = 0;

            gzFile fileStream;
            openFile (inFileName, fileStream, "rb"); // works also if file not compressed
            string line;
            while (getline (fileStream, line)) {
                strcpy(buffer, line.c_str());
            //while(fgets(buffer, MAX_BUFFER , stdin) != NULL) {

                buffer[strlen(buffer) - 1] =  '\0'; // do it before strtok
                memcpy(buffer2, buffer, sizeof(buffer2));

                i_row++;
                if (i_row % (arr_indName_outDispOrdering.size()*1000) == 0) { // (precisely, arr_indName_outDispOrdering.size()-1)
                    timer = time(NULL); stamp = ctime(&timer); stamp[strlen(stamp)-1] = '\0';
                    printf("%s: i_l2=%d\n",stamp,i_row/arr_indName_outDispOrdering.size());
                }

                *arr_line = strtok(buffer, " ");
                if (string(*arr_line) != "HAP" && string(*arr_line) != "pos") {

                    i_recipient_strain_of_this_pos++;

                    char  *p;
                    const char * tab = "\t";
                    int pos;

                    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
                    // prepare hash_site_by_site_prob of each site (rows)
                    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

                    // get pos & cnt_recipient
                    cnt_recipient = -1;
                    p = strstr(*arr_line, tab);
                    if (p != NULL) {
                        pos = atoi(strtok(*arr_line, tab));
                        cnt_recipient = atoi(strtok(NULL, tab));

                        if (verbose) {
                            //printf("%d\t%d\n",pos,cnt_recipient);
                        }
                    } else {
                        pos = atoi(*arr_line);
                        cnt_recipient = 1;
                        for (i = 1; ; i++) { 
                            if (NULL == (*(arr_line+i) = strtok(NULL , " "))){
                                break;
                            }
                            cnt_recipient++;
                        }
                        if (verbose) {
                            //printf("%d\n",cnt_recipient);
                        }
                    }

                    recipient_name = hash_strainIND2Name[cnt_recipient];

                    // skip the 1st column, and read all others 
                    *arr_line = strtok(buffer2, " ");
                    for (i = 1; ; i++) {
                        int cnt_donor = -1;
                        if (NULL == (*(arr_line+i) = strtok(NULL , " "))){
                            break;
                        }

                        if (type_painting == 2) {
                            cnt_donor = i;
                        } else {
                            if (i < cnt_recipient) {
                                cnt_donor = i;
                            } else {
                                cnt_donor = i+1;
                            }
                        }

                        donor_name = hash_strainIND2Name[cnt_donor];

                        //
                        // record site_by_site prob
                        // 
                        if (has_pairkey_intstring2int( hash_pos_missing_ind, pair<int,string>(pos,recipient_name) )) {
                            exclude_ind_flag = true;
                        } else if (has_pairkey_intstring2int( hash_pos_missing_ind, pair<int,string>(pos,donor_name) )) {
                            exclude_ind_flag = true;
                        } else {
                            exclude_ind_flag = false;
                        }

                        if (exclude_ind_flag == false) {
                            hash_site_by_site_prob[pair<int,int>(cnt_recipient,cnt_donor)] = atof(*(arr_line+i));
                        } else {
                            if ( !has_pairkey_int2double( hash_site_by_site_prob, pair<int,int>(cnt_recipient,cnt_donor) ) ) {
                                hash_site_by_site_prob[pair<int,int>(cnt_recipient,cnt_donor)] = 0;
                            }
                        }

                        if (verbose) {
                            if (exclude_ind_flag == false) {
                                pos_sum_distScore_included += square(atof(*(arr_line+i)) - hash_average_prob[pair<int,int>(cnt_recipient,cnt_donor)]);
                                pos_i_included++;
                            } else {
                                pos_sum_distScore_excluded += square(atof(*(arr_line+i)) - hash_average_prob[pair<int,int>(cnt_recipient,cnt_donor)]);
                                pos_i_excluded++;
                            }
                        }

                    } // for column


                    if (type_painting == 2 && (i_recipient_strain_of_this_pos != arr_indName_outDispOrdering.size()-1)) {
                        continue;
                    } else if (type_painting == 1 && (i_recipient_strain_of_this_pos != arr_indName_outDispOrdering.size())) {
                        continue;
                    } else {
                        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
                        // after hash_site_by_site_prob of this site was prepared above, 
                        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

                        donorInfoContent = 0;

                        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
                        // set diagonal and adjust each row
                        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                        for (i=0; i<arr_indName_eachOrdering.size(); i++) {
                            cnt_recipient = i+1;

                            // diagonal is always 0
                            hash_site_by_site_prob[pair<int,int>(cnt_recipient,cnt_recipient)] = 0;
                            hash_average_prob[pair<int,int>(cnt_recipient,cnt_recipient)] = 0;

                            rowsum_prob = 0;
                            for (j=0; j<arr_indName_eachOrdering.size(); j++) {
                                cnt_donor = j+1;

                                // 
                                // the following "if-else" is added to implement orderings-based conditioning
                                //   where donor strains are variable
                                //
                                if ( has_pairkey_int2double( hash_site_by_site_prob, pair<int,int>(cnt_recipient,cnt_donor) ) ) {
                                    // calculate rowsum_prob 
                                    // (cells with missing data were set to be 0 above)
                                    rowsum_prob += hash_site_by_site_prob[pair<int,int>(cnt_recipient,cnt_donor)];
                                } else {
                                    rowsum_prob += 0;
                                }
                            }

                            for (j=0; j<arr_indName_eachOrdering.size(); j++) {
                                cnt_donor = j+1;
                                donor_name = hash_strainIND2Name[cnt_donor];

                                //  
                                //  the following "if" is added to implement orderings-based conditioning
                                //    where donor strains are variable
                                //          the 1st recipient is not analyzed
                                // 
                                if (!(type_painting == 2 && cnt_recipient == 1)) {
                                    if ( has_pairkey_int2double( hash_site_by_site_prob, pair<int,int>(cnt_recipient,cnt_donor) ) ) {

                                        //  adjust each row
                                        //    the first recipient (row) is skipped because it is not analyzed

                                        // if there is no missing individual
                                        if (pos2missingIndFile == NULL) {
                                            if (rowsum_prob == 0) {
                                                fprintf(stderr, "Error: rowsum_prob is 0 (hash_site_by_site_prob) for recipient=%s, donor=%s\n", 
                                                    recipient_name.c_str(), donor_name.c_str());
                                                exit(1);
                                            } else {
                                                hash_site_by_site_prob[pair<int,int>(cnt_recipient,cnt_donor)] /= rowsum_prob;
                                            }
                                        // if there is missing individual
                                        } else {
                                            if (rowsum_prob == 0) {
                                                hash_site_by_site_prob[pair<int,int>(cnt_recipient,cnt_donor)] = -9;
                                            } else {
                                                hash_site_by_site_prob[pair<int,int>(cnt_recipient,cnt_donor)] /= rowsum_prob;
                                            }
                                        }

                                    }
                                }

                                //if ( has_pairkey_int2double( hash_site_by_site_prob, pair<int,int>(cnt_recipient,cnt_donor) ) ) {
                                //    double value = hash_site_by_site_prob[pair<int,int>(cnt_recipient,cnt_donor)];
                                //    donorInfoContent += value*value;
                                //}

                            } // donor loop

                        } // recipient loop


                        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
                        // calculate and save distScore of this site 
                        //   using the site_by_site and average matrices
                        //     according to outDisp ordering
                        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                        distScore_per_mat = 0;

                        //
                        // store score of each row as a vector
                        //   for bootstrapping of score of each row 
                        //
                        vector<double> arr_score_each_row; 
                        
                        for (i=0; i<arr_indName_eachOrdering.size(); i++) {

                            double distScore_per_row = 0;

                            cnt_recipient = i+1;
                            recipient_name = hash_strainIND2Name[cnt_recipient];

                            //
                            // orderings-based conditioning
                            //   skip the first recipient (row) in this distance calculation
                            //
                            if (type_painting == 2 && cnt_recipient==1) {
                                continue;
                            }

                            // calculate distScore_per_row
                            for (j=0; j<arr_indName_eachOrdering.size(); j++) {
                                cnt_donor = j+1;
                                donor_name = hash_strainIND2Name[cnt_donor];

                                if (!has_pairkey_int2double( hash_average_prob, pair<int,int>(cnt_recipient,cnt_donor))) {
                                    skip_calc_flag = true;
                                } else if (hash_site_by_site_prob[pair<int,int>(cnt_recipient,cnt_donor)] == -9) {
                                    skip_calc_flag = true;
                                } else if (hash_average_prob[pair<int,int>(cnt_recipient,cnt_donor)] == -9) {
                                    skip_calc_flag = true;
                                } else {
                                    if (donor_recipient_constraintFile != NULL && donor_recipient_constraintFile[0] != '\0') { // under development
                                        if (has_key_string2int(hash_constrained_donors, donor_name) && 
                                            has_key_string2int(hash_constrained_recipients, recipient_name) ) {
                                            skip_calc_flag = false; // use only this combination of donor and recipient
                                        } else {
                                            skip_calc_flag = true;
                                        }
                                    } else {
                                        skip_calc_flag = false;
                                    }
                                }

                                if (skip_calc_flag == false) {
                                    double each_score = square(
                                        (hash_site_by_site_prob[pair<int,int>(cnt_recipient,cnt_donor)] - 
                                         hash_average_prob[pair<int,int>(cnt_recipient,cnt_donor)])
                                        );
                                    distScore_per_row += each_score;
                                    distScore_per_mat += each_score;
                                }

                            } // donor

                            arr_score_each_row.push_back(distScore_per_row);

                        } // recipient


                        // 
                        // output 
                        //

                        // original distScore_per_mat
                        fprintf(fh_out, "%d\t%.13lf", pos, distScore_per_mat);
                        //fprintf(fh_out, "%d\t%.13lf\t%.13lf\n", pos, distScore_per_mat, donorInfoContent);
                        
                        // bootstrapping of score of each row 
                        double distScore_per_mat;
                        for (i = 0; i < N_BOOTSTRAP; i++) {
                            double bootstrapped_distScore_per_mat = 0;
                            for (j = 0; j < arr_score_each_row.size(); j++) {
                                int i_rand = getRandom(0, arr_score_each_row.size()-1);
                                bootstrapped_distScore_per_mat += arr_score_each_row[i_rand];
                            }
                            fprintf(fh_out, "\t%.13lf", bootstrapped_distScore_per_mat);
                        }
                        fprintf(fh_out, "\n");


                        // 
                        // prepartion for the next 
                        //
                        i_recipient_strain_of_this_pos = 0;

                        //
                        // verbose
                        //
                        if (verbose) {
                            fprintf(fh_inc_exc,"%d\t%lf",pos, pos_sum_distScore_included/pos_i_included);

                            if (pos_i_excluded > 0) {
                                fprintf(fh_inc_exc,"\t%lf\n", pos_sum_distScore_excluded/pos_i_excluded);
                            } else {
                                fprintf(fh_inc_exc,"\tNA\n");
                            }

                            pos_i_included = 0;
                            pos_i_excluded = 0;
                            pos_sum_distScore_included = 0;
                            pos_sum_distScore_excluded = 0;
                        }

                    } // after hash_site_by_site_prob of this site was prepared 

                } // skip headers

            } // read lines

        } else if (strcmp(loop_part,LOOP_030) == 0) {

            //
            // read positions to be processed
            //
            sprintf( fname, "%s/%s", dir_results, results_summary_pos ); 

            fh = fopen_wrapper(fname, "r");
            fgets(buffer, MAX_BUFFER, fh); // skip the header
            while (!feof(fh)) {
                if (fgets(buffer, MAX_BUFFER, fh) != NULL) {
                    buffer[strlen(buffer) - 1] =  '\0';

                    int pos;
                    string type; // top, middle, bottom

                    *arr_line = strtok(buffer , "\t");
                    pos = atoi(*arr_line);

                    for (i = 1; ; i++) { 
                        if (NULL == (*(arr_line+i) = strtok(NULL , "\t"))){
                            break;
                        }
                        if (i == 2) {
                            type = string(*(arr_line+i)); 
                            break;
                        }
                    }

                    hash_summaryPos2Type[pos] = type;
                }
            }
            fclose(fh);	

            // ************************************************************************
            // calculate and output Sij - Mj matrix (hash_site_minus_ave)
            //    only for the respresentative sites stored in hash_summaryPos2Type
            // ************************************************************************
            int i_row = 0;

            if (suffix == NULL) {
                sprintf( fname, "%s/%s", dir_each_ordering, out_each_dir_site_minus_average_matrix ); 
            } else {
                sprintf( fname, "%s/%s%s", dir_each_ordering, out_each_dir_site_minus_average_matrix, suffix ); 
            }

            fh_out = fopen_wrapper(fname, "w");

            gzFile fileStream;
            openFile (inFileName, fileStream, "rb"); // works also if file not compressed
            string line;
            while (getline (fileStream, line)) {
                strcpy(buffer, line.c_str());
            //while(fgets(buffer, MAX_BUFFER , stdin) != NULL) {

                buffer[strlen(buffer) - 1] =  '\0'; // do it before strtok
                memcpy(buffer2, buffer, sizeof(buffer2));

                i_row++;
                if (i_row % (arr_indName_outDispOrdering.size()*1000) == 0) { // (precisely, arr_indName_outDispOrdering.size()-1)
                    timer = time(NULL); stamp = ctime(&timer); stamp[strlen(stamp)-1] = '\0';
                    printf("%s: i_l3=%d\n",stamp,i_row/arr_indName_outDispOrdering.size());
                }

                *arr_line = strtok(buffer, " ");
                if (string(*arr_line) != "HAP" && string(*arr_line) != "pos") {

                    char  *p;
                    const char * tab = "\t";
                    int pos;

                    // get pos & cnt_recipient
                    cnt_recipient = -1;
                    p = strstr(*arr_line, tab);
                    if (p != NULL) {
                        pos = atoi(strtok(*arr_line, tab));
                        cnt_recipient = atoi(strtok(NULL, tab));

                        if (verbose) {
                            //printf("%d\t%d\n",pos,cnt_recipient);
                        }
                    } else {
                        pos = atoi(*arr_line);
                        cnt_recipient = 1;
                        for (i = 1; ; i++) { 
                            if (NULL == (*(arr_line+i) = strtok(NULL , " "))){
                                break;
                            }
                            cnt_recipient++;
                        }
                        if (verbose) {
                            //printf("%d\n",cnt_recipient);
                        }
                    }

                    // only for the respresentative sites stored in hash_summaryPos2Type
                    if (!has_key_int2string(hash_summaryPos2Type, pos)) {
                        continue;
                    }

                    recipient_name = hash_strainIND2Name[cnt_recipient];

                    // skip the 1st column, and read all others 
                    *arr_line = strtok(buffer2, " ");
                    for (i = 1; ; i++) {
                        int cnt_donor = -1;
                        if (NULL == (*(arr_line+i) = strtok(NULL , " "))){
                            break;
                        }

                        if (type_painting == 2) {
                            cnt_donor = i;
                        } else {
                            if (i < cnt_recipient) {
                                cnt_donor = i;
                            } else {
                                cnt_donor = i+1;
                            }
                        }

                        if (hash_average_prob[pair<int,int>(cnt_recipient,cnt_donor)] == -9) {
                            skip_calc_flag = true;
                        } else if (atof(*(arr_line+i)) == -9) {
                            skip_calc_flag = true;
                        } else {
                            skip_calc_flag = false;
                        }
                        // (constraint => look at specific rows and columns in this matrix)

                        if (skip_calc_flag == false) {
                            hash_site_minus_ave[pair<int,int>(cnt_recipient,cnt_donor)] = 
                                atof(*(arr_line+i)) - hash_average_prob[pair<int,int>(cnt_recipient,cnt_donor)];
                        }

                    } // for column

                    //
                    // save a row of this recipient in Sij - Mj matrix 
                    //   2nd column: recipient name is saved
                    //   3rd column and after that: according to outDisp ordering of donors
                    // 
                    fprintf(fh_out, "%d %s",pos,recipient_name.c_str());
                    for (i=0; i<arr_indName_outDispOrdering.size(); i++) {
                        cnt_donor = hash_strainName2IND[arr_indName_outDispOrdering[i]];

                        if ( !has_pairkey_int2double( hash_site_minus_ave, pair<int,int>(cnt_recipient,cnt_donor) ) ) {
                            //if ( hash_site_minus_ave.count(pair<int,int>(cnt_recipient,cnt_donor)) == 0) {
                            fprintf(fh_out, " 0");
                        } else {
                            fprintf(fh_out, " %.14e", hash_site_minus_ave[pair<int,int>(cnt_recipient,cnt_donor)]);
                        }
                    }
                    fprintf(fh_out, "\n");


                } // skip headers

            } // read lines

        } // loop 2 or 3

    } // loop 1 (create average) or not

    if (verbose) {
        fclose(fh_inc_exc);
    }
    fclose(fh_out);
    free(arr_line);

    timer = time(NULL); stamp = ctime(&timer); stamp[strlen(stamp)-1] = '\0';
    printf("%s: %s was created\n", stamp, fname);

    return 0;


}
