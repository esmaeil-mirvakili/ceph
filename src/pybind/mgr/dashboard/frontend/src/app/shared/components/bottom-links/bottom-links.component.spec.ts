import { HttpClientTestingModule } from '@angular/common/http/testing';
import { ComponentFixture, TestBed } from '@angular/core/testing';

import { configureTestBed } from '../../../../testing/unit-test-helper';
import { CephReleaseNamePipe } from './../../pipes/ceph-release-name.pipe';
import { DocComponent } from './../doc/doc.component';
import { BottomLinksComponent } from './bottom-links.component';

describe('BottomLinksComponent', () => {
  let component: BottomLinksComponent;
  let fixture: ComponentFixture<BottomLinksComponent>;

  configureTestBed({
    imports: [HttpClientTestingModule],
    declarations: [BottomLinksComponent, DocComponent],
    providers: [CephReleaseNamePipe]
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(BottomLinksComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
