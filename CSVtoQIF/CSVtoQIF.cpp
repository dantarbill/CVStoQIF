// CSVtoQIF.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int INPUT_BUFF_SIZE = 1024;
const int MAX_COLUMNS     = 20;
const int COLUMN_NAME_LEN = 20;
const int MEMO_STR_LEN    = 32;

#define CSV_DELIMITERS         ",\n"
#define HEADER_LINE            "!Type:Invst"
#define END_TRANSACTION        "^"
#define IMPORT_ALL_TXFR        "!Option:AllXfr"
#define ACTIVITY_BEFORE_TAX    "Before-Tax"
#define ACTIVITY_COMPANY_MATCH "Company Match"
#define ACTIVITY_CONTRIBUTIONS "Nonelective Contributions"
#define ACTIVITY_WITHDRAWLS    "Withdrawals"
#define SF_PREPEND             "SF "
#define ACTION_SELLX           "SellX"
#define ACTION_BUYX            "BuyX"
#define ACTION_BUY             "Buy"
#define CASH_TSFR_ACCT         "Cash"

#define FIELD_ID_IGNORE     'i'
#define FIELD_ID_DATE       'D'
#define FIELD_ID_ACTION     'N'
#define FIELD_ID_SECURITY   'Y'
#define FIELD_ID_PRICE      'I'
#define FIELD_ID_QUANTITY   'Q'
#define FIELD_ID_AMOUNT     'T'
#define FIELD_ID_CLEARED    'C'
#define FIELD_ID_MEMO       'M'
#define FIELD_ID_COMMISSION 'O'
#define FIELD_ID_TXFR_ACCT  'L'
#define FIELD_ID_TXFR_AMNT  '$'
/*
Items for Investment Accounts

Field 	Indicator Explanation
D 	Date
N 	Action
Y 	Security
I 	Price
Q 	Quantity (number of shares or split ratio)
T 	Transaction amount
C 	Cleared status
P 	Text in the first line for transfers and reminders
M 	Memo
O 	Commission
L 	Account for the transfer
$ 	Amount transferred
^ 	End of the entry

Column headers from State Farm...
i   VALUATION DATE   (ignore)
D   POSTING DATE     Date
M   ACTIVITY TYPE  * Memo
i   PLAN             "State Farm 401(k) Savings Plan" s/b "State Farm 401(k)" (ignore)
i   ACCOUNT          (Same as ACTIVITY TYPE (ignore))
Y   FUND             Security Name (Prepend "SF ")
T   AMOUNT         * Amount
I   FUND NAV/PRICE   Price
Q   FUND UNITS       Quantity
N   (derived)        Action  ( BuyX if T > 0 and M=Before-Tax, SellX T < 0)
O   (derived)        Commission (0 if not found)
C   (derived)        Cleared Status (X if not found)
L   (derived)        Account for the transfer (cash if M=Before-Tax)
$   (derived)        Amount transferred (=T if M=Before-Tax)

Investment Example

Transaction Item 	Comment (not in file)
!Type:Invst 	Header line
D8/25/93 	Date
NShrsIn 	Action (optional)
Yibm4 	Security
I11.260 	Price
Q88.81 	Quantity
CX 	Cleared Status
T1,000.00 	Amount
MOpening 	Balance Memo
^ 	End of the transaction
D8/25/93 	Date
NBuyX 	Action
Yibm4 	Security
I11.030 	Price
Q9.066 	Quantity
T100.00 	Amount
MEst. price as of 8/25/93 	Memo
L[CHECKING] 	Account for transfer
$100.00 	Amount transferred
^ 	End of the transaction*/


int main( int   argc
        , char *argv[]
        )
{
    if (argc < 2 )
    {
        printf ("Include CSV filename on command line\n");
        return 0;
    }
    
    char * csvFilename = argv[1];

    printf( "CSV Filename:%s\n"
          , csvFilename
          );

    // Do something tacky like putting a .qif at the end of the string
    // like blah.csv.qif
    // char qifFilename[];

    FILE * csvFile = fopen( csvFilename
                          , "r"
                          );
    char input[INPUT_BUFF_SIZE + 1];

    // Get the header line...
    if (fgets( input
             , INPUT_BUFF_SIZE
             , csvFile
             ) != NULL
       )     
    {
        printf( "%s\n"
              , input
              );
        
        char columnName[MAX_COLUMNS][COLUMN_NAME_LEN];
        char fieldID   [MAX_COLUMNS];
        char * strPtr = strtok( input
                              , CSV_DELIMITERS
                              );
        int   i               = 0;
        int   columnCount     = 0;
        bool  commissionFound = false;
        bool  clearedFound    = false;
        bool  actionFound     = false;
        bool  txfrAcctFound   = false;
        bool  txfrAmtFound    = false;
        bool  prePendSF       = false;

        // Get the field IDs for each column
        for( i = 0; (strPtr != nullptr) && (i < MAX_COLUMNS); i++ )
        {
            strncpy( columnName[i]
                   , strPtr
                   , COLUMN_NAME_LEN
                   );

            if      (_strcmpi( strPtr, "Date"              ) == 0)  fieldID[i] = FIELD_ID_DATE;
            else if (_strcmpi( strPtr, "POSTING DATE"      ) == 0)  fieldID[i] = FIELD_ID_DATE;
            else if (_strcmpi( strPtr, "VALUATION DATE"    ) == 0)  fieldID[i] = FIELD_ID_IGNORE;
            else if (_strcmpi( strPtr, "Memo"              ) == 0)  fieldID[i] = FIELD_ID_MEMO;
            else if (_strcmpi( strPtr, "ACTIVITY TYPE"     ) == 0)  fieldID[i] = FIELD_ID_MEMO;
            else if (_strcmpi( strPtr, "ACCOUNT"           ) == 0)  fieldID[i] = FIELD_ID_IGNORE;
            else if (_strcmpi( strPtr, "PLAN"              ) == 0)  fieldID[i] = FIELD_ID_IGNORE;
            else if (_strcmpi( strPtr, "FUND"              ) == 0) {fieldID[i] = FIELD_ID_SECURITY; prePendSF = true;}
            else if (_strcmpi( strPtr, "Security Name"     ) == 0)  fieldID[i] = FIELD_ID_SECURITY;
            else if (_strcmpi( strPtr, "Investment Action" ) == 0)  fieldID[i] = FIELD_ID_ACTION;
            else if (_strcmpi( strPtr, "Commission"        ) == 0)  fieldID[i] = FIELD_ID_COMMISSION; // 0 if not found
            else if (_strcmpi( strPtr, "Amount"            ) == 0)  fieldID[i] = FIELD_ID_AMOUNT;
            else if (_strcmpi( strPtr, "Price"             ) == 0)  fieldID[i] = FIELD_ID_PRICE;
            else if (_strcmpi( strPtr, "FUND NAV/PRICE"    ) == 0)  fieldID[i] = FIELD_ID_PRICE;
            else if (_strcmpi( strPtr, "Quantity"          ) == 0)  fieldID[i] = FIELD_ID_QUANTITY;
            else if (_strcmpi( strPtr, "FUND UNITS"        ) == 0)  fieldID[i] = FIELD_ID_QUANTITY;
            else if (_strcmpi( strPtr, "CLEARED"           ) == 0)  fieldID[i] = FIELD_ID_CLEARED;   // X if not found
            else if (_strcmpi( strPtr, "Transfer Account"  ) == 0)  fieldID[i] = FIELD_ID_TXFR_ACCT; // Cash if M=ACTIVITY_BEFORE_TAX
            else if (_strcmpi( strPtr, "Amount Transfered" ) == 0)  fieldID[i] = FIELD_ID_TXFR_AMNT; // T if M=ACTIVITY_BEFORE_TAX
            else fieldID[i] = FIELD_ID_IGNORE;

            switch(fieldID[i])
            {
            case FIELD_ID_COMMISSION: commissionFound = true; break;
            case FIELD_ID_CLEARED:    clearedFound    = true; break;
            case FIELD_ID_ACTION:     actionFound     = true; break;
            case FIELD_ID_TXFR_ACCT:  txfrAcctFound   = true; break;
            case FIELD_ID_TXFR_AMNT:  txfrAmtFound    = true; break;
            default:
                 break;
            } // switch fieldID

            strPtr = strtok( nullptr
                           , CSV_DELIMITERS
                           );
        }
        columnCount = i;

        // Print'em all out because I don't trust myself...
        for ( i = 0; i < columnCount; i++ )
        {
            if ( fieldID[i] != FIELD_ID_IGNORE )
            {
                printf( "%c %s\n"
                      , fieldID[i]
                      , columnName[i]
                      );
            } // if not ignore
        } // for each column

        // Open the output file qifFilename
        // output the printfs to the output file.

        printf( "%s\n"
               , HEADER_LINE 
               );

        float amount          = 0.0;
        char  amountStr[MEMO_STR_LEN];
        char  memoStr  [MEMO_STR_LEN];
        
        // Read the rest of the file...
        while (fgets( input
                    , INPUT_BUFF_SIZE
                    , csvFile
                    ) != NULL
              )
        {
            amount  = 0.0;
            memset( amountStr, 0, MEMO_STR_LEN );
            memset( memoStr  , 0, MEMO_STR_LEN );

            strPtr = strtok( input
                            , CSV_DELIMITERS
                            );

            for (i = 0; (strPtr != nullptr) && (i < columnCount); i++)
            {
                switch(fieldID[i])
                {
                case FIELD_ID_AMOUNT:
                    amount = strtof( strPtr, nullptr );
                    strncpy( amountStr, strPtr, MEMO_STR_LEN - 1 );
                    break;

                case FIELD_ID_MEMO:
                    strncpy( memoStr, strPtr, MEMO_STR_LEN - 1 );
                    break;
                default:
                    break;
                } // switch fieldID

                if ( fieldID[i] != FIELD_ID_IGNORE )
                {
                    if (  fieldID[i] == FIELD_ID_SECURITY
                       && prePendSF
                       )
                    {
                        printf( "%c%s%s\n"
                              , fieldID[i]
                              , SF_PREPEND
                              , strPtr
                              );
                    }
                    else
                    { 
                        printf( "%c%s\n"
                              , fieldID[i]
                              , strPtr
                              );
                    }
                }
                strPtr = strtok( nullptr
                               , CSV_DELIMITERS
                               );
            } // for each column

            // Deal with derived columns...

            // Derive action from the amount being positive or negative...
            // FIELD_ID_ACTION
            if(!actionFound)
            {
                if ( amount >= 0.0 )
                {
                    if ( _strcmpi( memoStr, ACTIVITY_BEFORE_TAX ) == 0 )
                    {
                        printf( "%c%s\n"
                              , FIELD_ID_ACTION
                              , ACTION_BUYX
                              );
                    }
                    else
                    {
                        printf( "%c%s\n"
                              , FIELD_ID_ACTION
                              , ACTION_BUY
                              );
                    }
                }
                else
                {
                    printf( "%c%s\n"
                          , FIELD_ID_ACTION
                          , ACTION_SELLX
                          );
                }
            } // if !actionFound

            // I think commision is a required field...
            // FIELD_ID_COMMISSION
            if(!commissionFound)
            {
                printf( "%c%s\n"
                        , FIELD_ID_COMMISSION
                        , "0.0"
                        );
            }

            // Mark 'em all cleared...
            // FIELD_ID_CLEARED
            if(!clearedFound)
            {
                printf( "%c%s\n"
                        , FIELD_ID_CLEARED
                        , "X"
                        );
            }
       
            // Deal with cash transfers...
            // FIELD_ID_TXFR_ACCT
            // FIELD_ID_TXFR_AMNT
            if (  _strcmpi( memoStr, ACTIVITY_BEFORE_TAX ) == 0
               || _strcmpi( memoStr, ACTIVITY_WITHDRAWLS ) == 0
               )
            {
                if(!txfrAcctFound)
                {
                    printf( "%c%s\n"
                            , FIELD_ID_TXFR_ACCT
                            , CASH_TSFR_ACCT
                            );
                }
                if(!txfrAmtFound)
                {
                    printf( "%c%s\n"
                            , FIELD_ID_TXFR_AMNT
                            , amountStr
                            );
                }
            } // if this needs a transfer account

            printf( "%s\n"
                  , END_TRANSACTION 
                  );

        } // while not EOF

        // close the output file

    } // if we got the header line

    return 0;
} // main() (yeah...it's ALL in main)

