#include <iostream>
#include <cstring>
#include <sqlite3.h>
#include <cstdlib>
#include <iomanip>
#include <ctime>
using namespace std;

bool isAdminLoggedIn = false;
bool isUserLoggedIn = false;

// Declare the Book structure
struct Book {
    string Id;
    string title;
    string author;
    string condition;
    int noCopies;
    Book *next;
};

struct BorrowedBook {
    string bookId;
    string borrower;
    time_t borrowedDate;
    time_t returnDate;
    BorrowedBook *next;
};
Book *library = NULL;
BorrowedBook *borrowedBooks = NULL;


// Function Prototypes
void addBook();
void searchBook();
void updateBookCondition();
void borrowBook();
void returnBook();
void displayBooks();
void calculateLateFee(time_t now, time_t returnDate);
void clearTerminal();
bool isValidDate(int year, int month, int day);
bool isValidAuthor(const string &author);
bool isValidCondition(const string &condition);
void adminLogin();
void userLogin();
void loginPrompt();
void viewBorrowedBooks();

// Clears the terminal
void clearTerminal() {
    system("clear");
}

// Adds a book by admin
void addBook() {
    Book *newBook = new Book;

    cout << "Enter Book ID: ";
    cin >> newBook->Id;
    cout << "Enter Book Title: ";
    cin.ignore();
    getline(cin, newBook->title);

    // Validate the author input
    while (true) {
        cout << "Enter Book Author: ";
        getline(cin, newBook->author);
        if (isValidAuthor(newBook->author)) {
            break;
        } else {
            cout << "Invalid author name. Please enter only alphabetic characters, spaces, hyphens, apostrophes, or periods.\n";
        }
    }

    // Validate the condition input
    while (true) {
        cout << "Enter Book Condition (New, Good, Fair, Poor): ";
        getline(cin, newBook->condition);
        if (isValidCondition(newBook->condition)) {
            break;
        } else {
            cout << "Invalid condition. Please enter one of the following: New, Good, Fair, Poor.\n";
        }
    }
    cout << "Enter Number of copies available: ";
    cin >> newBook->noCopies;

    newBook->next = NULL;

    if (library == NULL) {
        library = newBook;
    } else {
        Book *current = library;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = newBook;
    }

    cout << "Book added successfully!\n";
}

// Searches and displays a book by ID
void searchBook() {
    string searchId;
    cout << "Enter Book ID to search: ";
    cin >> searchId;

    Book *current = library;
    while (current != NULL) {
        if (current->Id == searchId) {
            cout << "Book Found:\n";
            cout << "ID: " << current->Id << "\n";
            cout << "Title: " << current->title << "\n";
            cout << "Author: " << current->author << "\n";
            cout << "Condition: " << current->condition << "\n";
            cout << "No of Copies Avaliable: " << current->noCopies << "\n";
            return;
        }
        current = current->next;
    }
    cout << "Book not found!\n";
}

// Updates the condition of a book
void updateBookCondition() {
    string searchId;
    cout << "Enter Book ID to update condition: ";
    cin >> searchId;

    Book *current = library;
    while (current != NULL) {
        if (current->Id == searchId) {
            cout << "Enter new condition for the book: ";
            cin.ignore();
            getline(cin, current->condition);
            cout << "Book condition updated successfully!\n";
            return;
        }
        current = current->next;
    }
    cout << "Book not found!\n";
}

// Borrows a book
void saveBorrowedBooks() {
    sqlite3 *db;
    int rc = sqlite3_open("library_management.db", &db);
    if (rc) {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return;
    }

    char *errMsg = NULL;
    string sql = "CREATE TABLE IF NOT EXISTS BorrowedBooks ("
                 "BookId TEXT, "
                 "Borrower TEXT, "
                 "BorrowedDate INTEGER, "
                 "ReturnDate INTEGER, "
                 "PRIMARY KEY(BookId, Borrower));";

    rc = sqlite3_exec(db, sql.c_str(), NULL, NULL, &errMsg);
    if (rc != SQLITE_OK) {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return;
    }

    BorrowedBook *borrowed = borrowedBooks;
    while (borrowed) {
        sql = "INSERT OR REPLACE INTO BorrowedBooks (BookId, Borrower, BorrowedDate, ReturnDate) VALUES (?, ?, ?, ?);";
        sqlite3_stmt *stmt;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, borrowed->bookId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, borrowed->borrower.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 3, borrowed->borrowedDate);
            sqlite3_bind_int64(stmt, 4, borrowed->returnDate);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        } else {
            cerr << "Failed to prepare insert statement: " << sqlite3_errmsg(db) << endl;
        }
        borrowed = borrowed->next;
    }

    sqlite3_close(db);
    cout << "Borrowed Books Data Saved Successfully!" << endl;
}

void borrowBook() {
    string searchId;
    cout << "Enter Book ID to borrow: ";
    cin >> searchId;

    Book *book = library;
    while (book != NULL) {
        if (book->Id == searchId) {  // Ensure we found the correct book
            if (book->noCopies <= 0) {
                cout << "No copies available for borrowing!\n";
                return;
            }

            // Create a new borrowed book entry
            BorrowedBook *newBorrowed = new BorrowedBook;
            newBorrowed->bookId = searchId;
            cout << "Enter borrower's name: ";
            cin.ignore();
            getline(cin, newBorrowed->borrower);
            time(&newBorrowed->borrowedDate);
            newBorrowed->returnDate = newBorrowed->borrowedDate + (7 * 24 * 60 * 60);
            newBorrowed->next = borrowedBooks;
            borrowedBooks = newBorrowed;

            // Decrement the book copy count
            book->noCopies--;
            cout << "Book borrowed successfully! Due date: " << ctime(&newBorrowed->returnDate);

            // Save borrowed book to the database
            saveBorrowedBooks();

            // Persist the updated noCopies in the database
            sqlite3 *db;
            int rc = sqlite3_open("library_management.db", &db);
            if (rc) {
                cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
                return;
            }

            string sql = "UPDATE Books SET noCopies = ? WHERE Id = ?;";
            sqlite3_stmt *stmt;
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, book->noCopies);
                sqlite3_bind_text(stmt, 2, searchId.c_str(), -1, SQLITE_STATIC);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            } else {
                cerr << "Failed to update book copies: " << sqlite3_errmsg(db) << endl;
            }
            sqlite3_close(db);

            return;
        }
        book = book->next;
    }

    cout << "Book not found!\n";
}

void returnBook() {
    string searchId, borrowerName;
    cout << "Enter Book ID to return: ";
    cin >> searchId;
    cout << "Enter borrower's name: ";
    cin.ignore();
    getline(cin, borrowerName);

    BorrowedBook *prev = NULL, *current = borrowedBooks;
    while (current != NULL) {
        if (current->bookId == searchId && current->borrower == borrowerName) {  // Verify borrower
            time_t now;
            time(&now);
            if (difftime(now, current->returnDate) > 0) {
                calculateLateFee(now, current->returnDate);
            }

            // Remove from linked list
            if (prev == NULL) {
                borrowedBooks = current->next; // Update head if it's the first element
            } else {
                prev->next = current->next; // Bypass the current node
            }
            delete current; // Free memory

            // Increase available copies in the book record
            Book *book = library;
            while (book != NULL) {
                if (book->Id == searchId) {
                    book->noCopies++;  // Increase count in memory
                    break;
                }
                book = book->next;
            }

            // Open Database Connection
            sqlite3 *db;
            int rc = sqlite3_open("library_management.db", &db);
            if (rc) {
                cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
                return;
            }

            // Remove the borrowed record from database
            string sqlDelete = "DELETE FROM BorrowedBooks WHERE bookId = ? AND borrower = ?;";
            sqlite3_stmt *stmtDelete;
            if (sqlite3_prepare_v2(db, sqlDelete.c_str(), -1, &stmtDelete, NULL) == SQLITE_OK) {
                sqlite3_bind_text(stmtDelete, 1, searchId.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmtDelete, 2, borrowerName.c_str(), -1, SQLITE_STATIC);
                sqlite3_step(stmtDelete);
                sqlite3_finalize(stmtDelete);
            } else {
                cerr << "Failed to prepare delete statement: " << sqlite3_errmsg(db) << endl;
            }

            // Update book copies in database
            string sqlUpdate = "UPDATE Books SET noCopies = noCopies + 1 WHERE Id = ?;";
            sqlite3_stmt *stmtUpdate;
            if (sqlite3_prepare_v2(db, sqlUpdate.c_str(), -1, &stmtUpdate, NULL) == SQLITE_OK) {
                sqlite3_bind_text(stmtUpdate, 1, searchId.c_str(), -1, SQLITE_STATIC);
                sqlite3_step(stmtUpdate);
                sqlite3_finalize(stmtUpdate);
            } else {
                cerr << "Failed to prepare update statement: " << sqlite3_errmsg(db) << endl;
            }

            sqlite3_close(db);
            cout << "Book returned successfully, copies updated, and database updated!\n";
            return;
        }
        prev = current;
        current = current->next;
    }
    cout << "Borrowed record not found or incorrect borrower name!\n";
}


// Calculates and displays late fee
void calculateLateFee(time_t now, time_t returnDate) {
    double secondsLate = difftime(now, returnDate);
    int daysLate = secondsLate / (60 * 60 * 24);
    double lateFee = daysLate * 0.50; // Assuming 0.50 currency units per day
    cout << "The book is returned " << daysLate << " days late. Late fee: " << fixed << setprecision(2) << lateFee << "\n";
}

// Validates the date
bool isValidDate(int year, int month, int day) {
    if (year < 1900 || month < 1 || month > 12 || day < 1 || day > 31)
        return false;

    if (month == 2) {
        bool isLeap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        if (day > (isLeap ? 29 : 28))
            return false;
    } else if (month == 4 || month == 6 || month == 9 || month == 11) {
        if (day > 30)
            return false;
    }

    return true;
}

// Validates the author string (alphabetic characters, spaces, hyphens, apostrophes, or periods)
bool isValidAuthor(const std::string &author) {
    for (size_t i = 0; i < author.length(); ++i) {
        char c = author[i];
        if (!std::isalpha(c) && c != ' ' && c != '-' && c != '\'' && c != '.') {
            return false;
        }
    }
    return true;
}

// Validates the book condition
bool isValidCondition(const std::string &condition) {
    const std::string validConditions[] = {"New", "Good", "Fair", "Poor"};
    for (size_t i = 0; i < 4; ++i) { // Iterate through the array
        if (condition == validConditions[i]) {
            return true;
        }
    }
    return false;
}

// Displays all books

void displayBooks() {
    if (library == NULL) {
        std::cout << "No books to display!\n";
        return;
    }

    // Table header
    std::cout << std::left << std::setw(10) << "ID"
              << std::setw(30) << "Title"
              << std::setw(20) << "Author"
              << std::setw(15) << "Condition"
              << std::setw(10) << "No Copies" << "\n";
    std::cout << std::setfill('-') << std::setw(85) << "" << std::setfill(' ') << "\n";

    Book *current = library;
    while (current != NULL) {
        std::cout << std::left << std::setw(10) << current->Id
                  << std::setw(30) << current->title
                  << std::setw(20) << current->author
                  << std::setw(15) << current->condition
                  << std::setw(10) << current->noCopies << "\n";
        current = current->next;
    }
}


void viewBorrowedBooks() {
    sqlite3 *db;
    int rc = sqlite3_open("library_management.db", &db);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    sqlite3_stmt *stmt;
    std::string sql = "SELECT BookId, Borrower, BorrowedDate, ReturnDate FROM BorrowedBooks;";

    // Load borrowed books
    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        // Table header
        std::cout << std::left << std::setw(10) << "Book ID"
                  << std::setw(20) << "Borrower"
                  << std::setw(25) << "Borrowed Date"
                  << std::setw(25) << "Return Date" << "\n";
        std::cout << std::setfill('-') << std::setw(80) << "" << std::setfill(' ') << "\n";

        // Iterate through the result set and print borrowed books details
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* bookId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* borrower = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            time_t borrowedDate = sqlite3_column_int64(stmt, 2);
            time_t returnDate = sqlite3_column_int64(stmt, 3);

            // Format dates to "dd/mm/yyyy"
            char borrowedDateStr[11], returnDateStr[11];
            strftime(borrowedDateStr, sizeof(borrowedDateStr), "%d/%m/%Y", localtime(&borrowedDate));
            strftime(returnDateStr, sizeof(returnDateStr), "%d/%m/%Y", localtime(&returnDate));

            // Display borrowed book information
            std::cout << std::left << std::setw(10) << bookId
                      << std::setw(20) << borrower
                      << std::setw(25) << borrowedDateStr
                      << std::setw(25) << returnDateStr << "\n";
        }
    } else {
        std::cerr << "Failed to retrieve borrowed books: " << sqlite3_errmsg(db) << std::endl;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}



bool checkIfExistsInDatabase(sqlite3 *db, const string &table, const string &column, const string &value);
void loadData();
void saveData();

// Check if a record exists in the database
bool checkIfExistsInDatabase(sqlite3 *db, const string &table, const string &column, const string &value) {
    sqlite3_stmt *stmt;
    string sql = "SELECT 1 FROM " + table + " WHERE " + column + " = ? LIMIT 1;";

    // Prepare SQL statement
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
        cerr << "Failed to prepare SQL statement. Error: " << sqlite3_errmsg(db) << endl;
        return false;
    }

    // Bind the value to the placeholder
    sqlite3_bind_text(stmt, 1, value.c_str(), -1, SQLITE_STATIC);

    // Execute the statement
    bool exists = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = true;
    }

    // Finalize statement
    sqlite3_finalize(stmt);

    return exists;
}

// Load data from the database into the linked list of books
void loadData() {
    sqlite3 *db;
    int rc = sqlite3_open("library_management.db", &db);
    if (rc) {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return;
    }

    sqlite3_stmt *stmt;
    string sql = "SELECT * FROM Books;";

    // Load books
    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Book *newBook = new Book();
            newBook->Id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            newBook->title = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
            newBook->author = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
            newBook->condition = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
            newBook->noCopies = sqlite3_column_int(stmt, 4);
            newBook->next = library; // Insert at the beginning
            library = newBook;
        }
    } else {
        cerr << "Failed to retrieve books: " << sqlite3_errmsg(db) << endl;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    cout << "Book data is added!" << endl;
}

// Save data from the linked list of books to the database
void saveData() {
    sqlite3 *db;
    int rc = sqlite3_open("library_management.db", &db);
    if (rc) {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return;
    }

    char *errMsg = NULL;

    // Create the required tables
    string sql =
        "CREATE TABLE IF NOT EXISTS Books ("
        "Id TEXT PRIMARY KEY, "
        "Title TEXT, "
        "Author TEXT, "
        "Condition TEXT, "
        "NoCopies INTEGER);";

    rc = sqlite3_exec(db, sql.c_str(), NULL, NULL, &errMsg);
    if (rc != SQLITE_OK) {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return;
    }
        Book *book = library;

    while (book) {
        // Check if the book already exists in the database
        if (!checkIfExistsInDatabase(db, "Books", "Id", book->Id)) {
            sql = "INSERT INTO Books (Id, Title, Author, Condition,NoCopies) VALUES (?, ?, ?, ?,?);";
            sqlite3_stmt *stmt;

            // Prepare the insert statement
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, book->Id.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, book->title.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, book->author.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 4, book->condition.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_int(stmt, 5, book->noCopies);


                // Execute the statement
                if (sqlite3_step(stmt) != SQLITE_DONE) {
                    cerr << "Error inserting data: " << sqlite3_errmsg(db) << endl;
                }
                sqlite3_finalize(stmt);
            } else {
                cerr << "Failed to prepare insert statement: " << sqlite3_errmsg(db) << endl;
            }
        }
        book = book->next; // Move to the next book in the list
    }

    sqlite3_close(db);
    cout << "Data Saved Successfully!" << endl;
}


// Main function
void adminLogin() {
    const string adminUsername = "admin";
    const string adminPassword = "123"; // You can modify this

    string username, password;
    cout << "Admin Login\n";
    cout << "Enter username: ";
    cin >> username;
    cout << "Enter password: ";
    cin >> password;

    if (username == adminUsername && password == adminPassword) {
        cout << "Admin login successful.\n";
        isAdminLoggedIn = true;
        isUserLoggedIn = false;

        while (isAdminLoggedIn) {
            int choice;
            cout << "\n\t********************************************\n";
            cout << "\t          Admin Menu - Library Management System          \n";
            cout << "\t********************************************\n";
            cout << "\t1. Display Books\n";
            cout << "\t2. Search Book\n";
            cout << "\t3. Add Book\n";
            cout << "\t4. Update Book Condition\n";
            cout << "\t5. View Borrowed Books\n";
            cout << "\t6. Logout\n";
            cout << "\t7. Exit\n";
            cout << "\tEnter your choice: ";
            cin >> choice;

            switch (choice) {
                case 1:
                    displayBooks();
                    break;
                case 2:
                    searchBook();
                    break;
                case 3:
                    addBook();
                    saveData();
                    break;
                case 4:
                    updateBookCondition();
                    break;
                case 5:
                    viewBorrowedBooks();
                    break;
                case 6:
                    isAdminLoggedIn = false;
                    cout << "Admin logged out.\n";
                    loginPrompt();
                    break;
                case 7:
                    cout << "Exiting the program. Goodbye!\n";
                    exit(0);
                default:
                    cout << "Invalid choice, please try again.\n";
            }
        }
    } else {
        cout << "Invalid username or password. Please try again.\n";
    }
}

void userLogin() {

    cout << "User login successful.\n";
    isUserLoggedIn = true;
    isAdminLoggedIn=false;

    while (isUserLoggedIn) {
        int choice;
        cout << "\n\t********************************************\n";
        cout << "\t          User Menu - Library Management System          \n";
        cout << "\t********************************************\n";
        cout << "\t1. Display Books\n";
        cout << "\t2. Search Book\n";
        cout << "\t3. Borrow Book\n";
        cout << "\t4. Return Book\n";
        cout << "\t5. Logout\n";
        cout << "\t6. Exit\n";
        cout << "\tEnter your choice: ";
        cin >> choice;

        switch (choice) {
            case 1:
                displayBooks();
                break;
            case 2:
                searchBook();
                break;
            case 3:
                borrowBook();
                saveData();
                break;
            case 4:
                returnBook();
                break;
            case 5:
                isUserLoggedIn = false;
                cout << "User logged out\n";
                loginPrompt();
                break;
            case 6:
                cout << "Exiting the program. Goodbye!\n";
                exit(0);
            default:
                cout << "Invalid choice, please try again.\n";
        }
    }
}
void loginPrompt() {
    int loginChoice;
    while (true) {
        // Initial login prompt
        cout << "\n\t********************************************\n";
        cout << "\t          Library Management System          \n";
        cout << "\t********************************************\n";
        cout << "\t1. Continue as User\n";
        cout << "\t2. Admin Login\n";
        cout << "\t3. Exit\n";
        cout << "\tEnter your choice: ";
        cin >> loginChoice;

        if (loginChoice == 1) {
            userLogin();
        } else if (loginChoice == 2) {
            adminLogin();
            break; // Exit login loop and proceed to menu
        } else if (loginChoice == 3) {
            cout << "Exiting the program. Goodbye!\n";
            exit(0); // Terminate the program
        } else {
            cout << "Invalid choice, please try again.\n";
        }
    }
}

// Main function


int main() {
    loadData();
    loginPrompt();
    return 0;
}
