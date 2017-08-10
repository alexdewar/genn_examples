
clear

pmdir = 'pm_dump';

nsnaps = length(dir(fullfile(pmdir,'snapshot*.png')));
snaps = NaN(10,36,nsnaps);
for i = 1:nsnaps
    snaps(:,:,i) = double(imread(fullfile('pm_dump',sprintf('snapshot%d.png',i))));
end

curview = double(imread(fullfile(pmdir,'current.png')));

ridf = csvread(fullfile(pmdir,'ridf.csv'));

[head,minval,whsn,diffs] = ridfheadmulti(curview,snaps,[],360);
fprintf('rotation: %g\nsnap: %d\nvalue: %g\n\n', head, whsn-1, minval);

disp('log file')
fid = fopen(fullfile(pmdir,'log.txt'),'r');
disp(fread(fid,'*char')')
fclose(fid);

figure(1);clf
plot([diffs(:,1), ridf(:,1)])